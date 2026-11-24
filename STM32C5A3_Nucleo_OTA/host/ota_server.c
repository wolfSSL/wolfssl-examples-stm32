/* ota_server.c
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Host-side OTA server for the NUCLEO-C5A3ZG demo.
 *
 * TLS 1.3, requires and verifies the device certificate, offers the ML-KEM-768
 * hybrid groups first. Protocol after the handshake:
 *   client: "OTA <current version>\n"
 *   server: "WBUP" | u32 image size | u32 image version (little endian)
 *   client: "GO\n" to receive the image (anything else declines it)
 *   server: the signed wolfBoot image
 *   client: "PING\n"  ->  server: "PONG\n"  (handshake benchmark)
 *
 * The served image is re-read on every request, so the operator can swap
 * the file between checks (v2, v3, tampered, downgrade).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>

#define WOLFBOOT_MAGIC        0x464C4F57u   /* "WOLF" */
#define HDR_VERSION_TYPE      0x0001u

static const char *image_path;

/* Version TLV from the wolfBoot manifest header (magic, size, then TLVs). */
static uint32_t image_version(const uint8_t *img, size_t len)
{
    size_t off = 8;
    uint32_t magic;

    if (len < 16)
        return 0;
    memcpy(&magic, img, 4);
    if (magic != WOLFBOOT_MAGIC)
        return 0;
    while (off + 4 <= len && off < 256) {
        uint16_t type = (uint16_t)(img[off] | (img[off + 1] << 8));
        uint16_t tlen = (uint16_t)(img[off + 2] | (img[off + 3] << 8));
        if (type == 0xFF || type == 0x00) {
            off++;
            continue;
        }
        if (type == HDR_VERSION_TYPE && tlen == 4 && off + 8 <= len) {
            uint32_t v;
            memcpy(&v, img + off + 4, 4);
            return v;
        }
        off += 4u + tlen;
    }
    return 0;
}

static uint8_t *load_image(size_t *len)
{
    FILE *f = fopen(image_path, "rb");
    uint8_t *buf;
    long sz;

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)sz);
    if (buf != NULL && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *len = (size_t)sz;
    return buf;
}

static void serve(WOLFSSL *ssl)
{
    char line[64];
    int n = 0, r;
    char c;

    while (n < (int)sizeof(line) - 1) {
        r = wolfSSL_read(ssl, &c, 1);
        if (r <= 0)
            return;
        if (c == '\n')
            break;
        line[n++] = c;
    }
    line[n] = 0;

    if (strcmp(line, "PING") == 0) {
        wolfSSL_write(ssl, "PONG\n", 5);
        return;
    }
    if (strncmp(line, "OTA ", 4) == 0) {
        size_t len = 0;
        uint8_t *img = image_path ? load_image(&len) : NULL;
        uint8_t hdr[12];
        uint32_t magic = 0x50554257u, size = (uint32_t)len, ver;

        ver = img ? image_version(img, len) : 0;
        if (img == NULL)
            size = 0;
        memcpy(hdr, &magic, 4);
        memcpy(hdr + 4, &size, 4);
        memcpy(hdr + 8, &ver, 4);
        printf("  device runs v%s, offering %s (v%u, %u bytes)\n", line + 4,
            image_path ? image_path : "(none)", ver, size);
        fflush(stdout);
        wolfSSL_write(ssl, hdr, sizeof(hdr));
        /* stream the image only when the device asks for it */
        n = 0;
        while (img != NULL && n < 3) {
            r = wolfSSL_read(ssl, line + n, 1);
            if (r <= 0)
                break;
            n++;
        }
        if (img != NULL && (n < 3 || memcmp(line, "GO\n", 3) != 0)) {
            printf("  device declined the image\n");
            free(img);
            img = NULL;
        }
        if (img != NULL) {
            size_t off = 0;
            while (off < len) {
                int chunk = (int)((len - off) > 4096 ? 4096 : (len - off));
                r = wolfSSL_write(ssl, img + off, chunk);
                if (r <= 0)
                    break;
                off += (size_t)r;
            }
            free(img);
        }
    }
}

int main(int argc, char **argv)
{
    int port = 11111, opt, lfd, on = 1;
    const char *certdir = "certs";
    char path[512];
    struct sockaddr_in sa;
    WOLFSSL_CTX *ctx;
    int groups[] = {
        WOLFSSL_X25519MLKEM768, WOLFSSL_SECP256R1MLKEM768, WOLFSSL_ML_KEM_768,
        WOLFSSL_ECC_X25519, WOLFSSL_ECC_SECP256R1
    };

    while ((opt = getopt(argc, argv, "p:f:c:")) != -1) {
        if (opt == 'p') port = atoi(optarg);
        else if (opt == 'f') image_path = optarg;
        else if (opt == 'c') certdir = optarg;
        else {
            fprintf(stderr, "usage: %s [-p port] [-f signed_image.bin] "
                "[-c certdir]\n", argv[0]);
            return 1;
        }
    }

    wolfSSL_Init();
    ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
    snprintf(path, sizeof(path), "%s/server-cert.pem", certdir);
    if (wolfSSL_CTX_use_certificate_file(ctx, path, WOLFSSL_FILETYPE_PEM) != 1)
        return fprintf(stderr, "bad %s\n", path), 1;
    snprintf(path, sizeof(path), "%s/server-key.pem", certdir);
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, path, WOLFSSL_FILETYPE_PEM) != 1)
        return fprintf(stderr, "bad %s\n", path), 1;
    snprintf(path, sizeof(path), "%s/ca-cert.pem", certdir);
    if (wolfSSL_CTX_load_verify_locations(ctx, path, NULL) != 1)
        return fprintf(stderr, "bad %s\n", path), 1;
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER |
        WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    wolfSSL_CTX_set_groups(ctx, groups, (int)(sizeof(groups) / sizeof(groups[0])));

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    sa.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(lfd, 4) != 0)
        return perror("bind/listen"), 1;
    printf("OTA server on port %d, image %s\n", port,
        image_path ? image_path : "(none)");
    fflush(stdout);

    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int fd = accept(lfd, (struct sockaddr *)&peer, &plen);
        WOLFSSL *ssl;
        struct timeval tv = { 15, 0 };
        if (fd < 0)
            continue;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        ssl = wolfSSL_new(ctx);
        wolfSSL_set_fd(ssl, fd);
        int ar = wolfSSL_accept(ssl);
        if (ar == WOLFSSL_SUCCESS) {
            printf("%s: TLS 1.3 %s, group %s, client certificate verified\n",
                inet_ntoa(peer.sin_addr), wolfSSL_get_cipher(ssl),
                wolfSSL_get_curve_name(ssl));
            serve(ssl);
        }
        else {
            char es[80];
            int e = wolfSSL_get_error(ssl, ar);
            printf("%s: handshake failed (%d %s)\n", inet_ntoa(peer.sin_addr),
                e, wolfSSL_ERR_error_string((unsigned long)e, es));
        }
        fflush(stdout);
        wolfSSL_shutdown(ssl);
        wolfSSL_free(ssl);
        close(fd);
    }
    return 0;
}
