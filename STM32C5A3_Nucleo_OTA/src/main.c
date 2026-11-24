/* main.c
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

/* NUCLEO-C5A3ZG secure OTA demo.
 *
 * wolfBoot verifies and boots this image. The image brings up wolfIP (DHCP),
 * connects to the OTA server over TLS 1.3 with an ML-KEM-768 hybrid key
 * exchange and ECDSA P-256 mutual authentication, downloads a signed image
 * into the UPDATE partition and triggers wolfBoot, which verifies the new
 * image before swapping it in.
 *
 * Console (USART2, ST-LINK VCP, 115200 8N1):
 *   u  check the server for an update (also the B1 user button)
 *   b  handshake benchmark over every key-exchange group
 *   t  install an image already staged in UPDATE (e.g. written over SWD)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "stm32c5a3xx.h"
#include "config.h"
#include "wolfip.h"
#include "stm32_eth.h"
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/memory.h>
#include "certs.h"
#include "ota_flash.h"

#ifndef OTA_SERVER_IP
#define OTA_SERVER_IP   TLS_SERVER_IP
#endif
#ifndef OTA_SERVER_PORT
#define OTA_SERVER_PORT TLS_SERVER_PORT
#endif
#ifndef OTA_GROUP
#define OTA_GROUP       WOLFSSL_X25519MLKEM768
#endif
#ifndef OTA_BENCH_ROUNDS
#define OTA_BENCH_ROUNDS 5
#endif

#define SYSCLK_HZ       144000000u
#define HSE_TIMEOUT     0x100000u
#define PSI_TIMEOUT     0x100000u
#define SW_TIMEOUT      0x010000u
#define NET_TIMEOUT_MS  10000u
/* DHCP probes the leased address (RFC 5227) before it binds */
#define DHCP_TIMEOUT_MS 20000u
#define OTA_MAGIC       0x50554257u     /* "WBUP" little-endian */
#define OTA_CHUNK       2048u

#define DEMCR           (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004u)

static struct wolfIP *IPStack;
static WOLFSSL_CTX *g_ctx;
static volatile uint32_t g_ms;
static uint32_t g_version;

void SystemInit(void) { }

time_t demo_time(time_t *t)
{
    time_t now = (time_t)DEMO_BUILD_EPOCH;
    if (t != NULL)
        *t = now;
    return now;
}

void SysTick_Handler(void)
{
    g_ms++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t t0 = g_ms;
    while ((g_ms - t0) < ms) { }
}

/* ------------------------------------------------------------------------ */
/* Clock, console, RNG (board bring-up shared with the wolfIP C5A3 port)    */
/* ------------------------------------------------------------------------ */
static void clock_init(void)
{
    uint32_t reg;
    volatile uint32_t timeout;

    /* RNG kernel clock CK48 <- HSIDIV3 (HSI / 3 = 48 MHz) */
    RCC->CR1 |= RCC_CR1_HSIDIV3ON;
    timeout = HSE_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--timeout != 0u)) { }
    reg = RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk;
    RCC->CCIPR2 = reg | RCC_CCIPR2_CK48SEL_1;

    /* HSE 48 MHz -> PSI -> PSIS 144 MHz. wolfBoot leaves HSE and PSIS
     * running when it restores SYSCLK to HSIDIV3, so these waits are short. */
    if ((RCC->CR1 & RCC_CR1_HSEON) == 0u)
        RCC->CR1 |= RCC_CR1_HSEON;
    timeout = HSE_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSERDY) == 0u) && (--timeout != 0u)) { }
    if (timeout == 0u)
        return;
    reg = RCC->CR2 & ~(RCC_CR2_PSIREFSRC | RCC_CR2_PSIREF | RCC_CR2_PSIFREQ);
    reg |= (RCC_CR2_PSIREF_2 | RCC_CR2_PSIREF_1) | RCC_CR2_PSIFREQ_0;
    RCC->CR2 = reg;
    RCC->CR1 |= RCC_CR1_PSISON;
    timeout = PSI_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_PSISRDY) == 0u) && (--timeout != 0u)) { }
    if (timeout == 0u)
        return;
    RCC->CFGR2 = 0u;
    reg = FLASH->ACR & ~FLASH_ACR_LATENCY;
    FLASH->ACR = reg | (4u << FLASH_ACR_LATENCY_Pos) | FLASH_ACR_PRFTEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != (4u << FLASH_ACR_LATENCY_Pos)) { }
    reg = RCC->CFGR1 & ~RCC_CFGR1_SW;
    RCC->CFGR1 = reg | (RCC_CFGR1_SW_0 | RCC_CFGR1_SW_1);
    timeout = SW_TIMEOUT;
    while (((RCC->CFGR1 & RCC_CFGR1_SWS) != (RCC_CFGR1_SWS_0 | RCC_CFGR1_SWS_1))
            && (--timeout != 0u)) { }
    if (timeout == 0u)
        return;
    reg = FLASH->ACR & ~FLASH_ACR_WRHIGHFREQ;
    FLASH->ACR = reg | (2u << FLASH_ACR_WRHIGHFREQ_Pos);
}

static void console_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (7u << GPIO_AFRL_AFSEL2_Pos) | (7u << GPIO_AFRL_AFSEL3_Pos);
    RCC->APB1LENR |= RCC_APB1LENR_USART2EN;
    (void)RCC->APB1LENR;
    USART2->CR1 = 0;
    USART2->BRR = SYSCLK_HZ / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

static void putc_(char c)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0u) { }
    USART2->TDR = (uint32_t)c & 0xFFu;
}

static void puts_(const char *s)
{
    while (*s) {
        if (*s == '\n')
            putc_('\r');
        putc_(*s++);
    }
}

static void putu(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u);
    puts_(&b[i]);
}

/* microseconds as "<ms>.<3 digits> ms" */
static void put_ms(uint32_t us)
{
    uint32_t frac = us % 1000u;
    putu(us / 1000u);
    putc_('.');
    putc_((char)('0' + frac / 100u));
    putc_((char)('0' + (frac / 10u) % 10u));
    putc_((char)('0' + frac % 10u));
    puts_(" ms");
}

static void put_ip(ip4 ip)
{
    putu((ip >> 24) & 0xFFu); putc_('.');
    putu((ip >> 16) & 0xFFu); putc_('.');
    putu((ip >> 8) & 0xFFu);  putc_('.');
    putu(ip & 0xFFu);
}

static int getc_nb(void)
{
    if ((USART2->ISR & USART_ISR_RXNE_RXFNE) == 0u)
        return -1;
    return (int)(USART2->RDR & 0xFFu);
}

static void rng_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    (void)RCC->AHB2ENR;
    RNG->CR = RNG_CR_RNGEN | RNG_CR_CED;
}

uint32_t wolfIP_getrandom(void)
{
    uint32_t timeout = 100000u;
    while ((RNG->SR & RNG_SR_DRDY) == 0u) {
        if ((RNG->SR & (RNG_SR_CECS | RNG_SR_SECS)) != 0u) {
            RNG->CR = 0;
            RNG->CR = RNG_CR_RNGEN | RNG_CR_CED;
        }
        if (--timeout == 0u)
            break;
    }
    return RNG->DR;
}

/* ------------------------------------------------------------------------ */
/* LED (LD2, PG1, active low) and B1 user button (PC13, active high)        */
/* ------------------------------------------------------------------------ */
static void io_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOGEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR;
    GPIOG->MODER = (GPIOG->MODER & ~(3u << (1u * 2u))) | (1u << (1u * 2u));
    GPIOG->BSRR = (1u << 1);
    GPIOC->MODER &= ~(3u << (13u * 2u));
}

static void led_set(int on)
{
    GPIOG->BSRR = on ? (1u << (1 + 16)) : (1u << 1);
}

static int button_pressed(void)
{
    return (GPIOC->IDR & (1u << 13)) != 0u;
}

/* v1: slow blink, v2: fast blink, v3+: double flash */
static void led_pattern(void)
{
    uint32_t t = g_ms;
    int on;

    if (g_version <= 1u)
        on = (t % 1000u) < 500u;
    else if (g_version == 2u)
        on = (t % 250u) < 125u;
    else
        on = ((t % 1000u) < 100u) || (((t % 1000u) >= 200u) &&
            ((t % 1000u) < 300u));
    led_set(on);
}

/* ------------------------------------------------------------------------ */
/* Ethernet (RMII) bring-up                                                 */
/* ------------------------------------------------------------------------ */
static void eth_pin(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    port->MODER = (port->MODER & ~(3u << (pin * 2u))) | (2u << (pin * 2u));
    port->OSPEEDR |= (3u << (pin * 2u));
    if (pin < 8u)
        port->AFR[0] = (port->AFR[0] & ~(0xFu << (pin * 4u))) | (af << (pin * 4u));
    else
        port->AFR[1] = (port->AFR[1] & ~(0xFu << ((pin - 8u) * 4u))) |
            (af << ((pin - 8u) * 4u));
}

static int eth_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOCEN |
                    RCC_AHB2ENR_GPIODEN | RCC_AHB2ENR_GPIOEEN |
                    RCC_AHB2ENR_GPIOGEN;
    RCC->APB3ENR |= RCC_APB3ENR_SBSEN;
    (void)RCC->APB3ENR;
    SBS->PMCR = (SBS->PMCR & ~SBS_PMCR_ETH1_SEL_PHY_Msk) | SBS_PMCR_ETH1_SEL_PHY_2;
    __DSB();
    /* NUCLEO-C5A3ZG RMII: AF10 except RXD0 (PC4) AF12 and RXD1 (PC5) AF13 */
    eth_pin(GPIOA, 1, 10);
    eth_pin(GPIOC, 1, 10);
    eth_pin(GPIOE, 12, 10);
    eth_pin(GPIOD, 1, 10);
    eth_pin(GPIOC, 4, 12);
    eth_pin(GPIOC, 5, 13);
    eth_pin(GPIOG, 11, 10);
    eth_pin(GPIOG, 13, 10);
    eth_pin(GPIOG, 12, 10);
    delay_ms(200);
    RCC->AHB1ENR |= RCC_AHB1ENR_ETH1EN | RCC_AHB1ENR_ETH1TXEN |
                    RCC_AHB1ENR_ETH1RXEN | RCC_AHB1ENR_ETH1CKEN;
    __DSB();
    delay_ms(1);
    RCC->AHB1RSTR |= RCC_AHB1RSTR_ETH1RST;
    __DSB();
    delay_ms(1);
    RCC->AHB1RSTR &= ~RCC_AHB1RSTR_ETH1RST;
    __DSB();
    delay_ms(10);
    return stm32_eth_init(wolfIP_getdev(IPStack), NULL);
}

static void net_poll(void)
{
    (void)wolfIP_poll(IPStack, g_ms);
}

/* ------------------------------------------------------------------------ */
/* Measurement: DWT cycle counter, wolfSSL heap peak, stack painting        */
/* ------------------------------------------------------------------------ */
static uint32_t g_heap_cur;
static uint32_t g_heap_peak;
extern uint32_t __StackLimit;
extern uint32_t __StackTop;
#define STACK_PAINT 0xA5A5A5A5u

static void *trk_malloc(size_t sz)
{
    uint32_t *p = (uint32_t *)malloc(sz + 8u);
    if (p == NULL)
        return NULL;
    p[0] = (uint32_t)sz;
    g_heap_cur += (uint32_t)sz;
    if (g_heap_cur > g_heap_peak)
        g_heap_peak = g_heap_cur;
    return p + 2;
}

static void trk_free(void *ptr)
{
    uint32_t *p;
    if (ptr == NULL)
        return;
    p = (uint32_t *)ptr - 2;
    g_heap_cur -= p[0];
    free(p);
}

static void *trk_realloc(void *ptr, size_t sz)
{
    void *n = trk_malloc(sz);
    if (n != NULL && ptr != NULL) {
        uint32_t old = ((uint32_t *)ptr - 2)[0];
        memcpy(n, ptr, old < sz ? old : sz);
        trk_free(ptr);
    }
    return n;
}

static void stack_paint(void)
{
    volatile uint32_t *p = &__StackLimit;
    uint32_t sp;
    __asm volatile ("mov %0, sp" : "=r"(sp));
    while ((uint32_t)p < sp - 256u)
        *p++ = STACK_PAINT;
}

static uint32_t stack_peak(void)
{
    uint32_t *p = &__StackLimit;
    while (p < &__StackTop && *p == STACK_PAINT)
        p++;
    return (uint32_t)&__StackTop - (uint32_t)p;
}

static uint32_t cyc_to_us(uint32_t cyc)
{
    return cyc / (SYSCLK_HZ / 1000000u);
}

/* ------------------------------------------------------------------------ */
/* TLS session over wolfIP                                                  */
/* ------------------------------------------------------------------------ */
struct tls_stats {
    uint32_t hs_us;
    uint32_t heap_peak;
    uint32_t stack_peak;
};

static int tls_ctx_init(void)
{
    g_ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    if (g_ctx == NULL)
        return -1;
    if (wolfSSL_CTX_load_verify_buffer(g_ctx, ca_cert_der, sizeof(ca_cert_der),
            WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS)
        return -2;
    if (wolfSSL_CTX_use_certificate_buffer(g_ctx, device_cert_der,
            sizeof(device_cert_der), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS)
        return -3;
    if (wolfSSL_CTX_use_PrivateKey_buffer(g_ctx, device_key_der,
            sizeof(device_key_der), WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS)
        return -4;
    wolfSSL_CTX_set_verify(g_ctx, WOLFSSL_VERIFY_PEER, NULL);
    wolfSSL_SetIO_wolfIP_CTX(g_ctx, IPStack);
    return 0;
}

static void tls_close(WOLFSSL *ssl, int fd)
{
    if (ssl != NULL) {
        wolfSSL_shutdown(ssl);
        wolfSSL_CleanupIO_wolfIP(ssl);
        wolfSSL_free(ssl);
    }
    if (fd >= 0)
        wolfIP_sock_close(IPStack, fd);
    net_poll();
}

static int tls_open(int group, WOLFSSL **out, int *out_fd, struct tls_stats *st)
{
    struct wolfIP_sockaddr_in addr;
    WOLFSSL *ssl = NULL;
    int fd, ret, err;
    uint32_t t0, c0, heap_base;

    *out = NULL;
    fd = wolfIP_sock_socket(IPStack, AF_INET, IPSTACK_SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = ee16(OTA_SERVER_PORT);
    addr.sin_addr.s_addr = ee32(atoip4(OTA_SERVER_IP));
    t0 = g_ms;
    for (;;) {
        ret = wolfIP_sock_connect(IPStack, fd, (struct wolfIP_sockaddr *)&addr,
            sizeof(addr));
        if (ret >= 0)
            break;
        if (ret != -WOLFIP_EAGAIN || (g_ms - t0) > NET_TIMEOUT_MS) {
            puts_("TCP connect failed\n");
            wolfIP_sock_close(IPStack, fd);
            return -2;
        }
        net_poll();
    }

    heap_base = g_heap_cur;
    g_heap_peak = g_heap_cur;
    stack_paint();
    ssl = wolfSSL_new(g_ctx);
    if (ssl == NULL) {
        tls_close(NULL, fd);
        return -3;
    }
    wolfSSL_set_groups(ssl, &group, 1);
    wolfSSL_UseKeyShare(ssl, (word16)group);
    wolfSSL_SetIO_wolfIP(ssl, fd);

    t0 = g_ms;
    c0 = DWT_CYCCNT;
    for (;;) {
        ret = wolfSSL_connect(ssl);
        if (ret == WOLFSSL_SUCCESS)
            break;
        err = wolfSSL_get_error(ssl, ret);
        if ((err != WOLFSSL_ERROR_WANT_READ && err != WOLFSSL_ERROR_WANT_WRITE)
                || (g_ms - t0) > NET_TIMEOUT_MS) {
            puts_("TLS handshake failed, error ");
            putu((uint32_t)-err);
            puts_("\n");
            tls_close(ssl, fd);
            return -4;
        }
        net_poll();
    }
    st->hs_us = cyc_to_us(DWT_CYCCNT - c0);
    st->heap_peak = g_heap_peak - heap_base;
    st->stack_peak = stack_peak();
    *out = ssl;
    *out_fd = fd;
    return 0;
}

static int tls_write_all(WOLFSSL *ssl, const void *buf, int len)
{
    int ret, err;
    uint32_t t0 = g_ms;
    for (;;) {
        ret = wolfSSL_write(ssl, buf, len);
        if (ret == len)
            return 0;
        err = wolfSSL_get_error(ssl, ret);
        if ((err != WOLFSSL_ERROR_WANT_WRITE && err != WOLFSSL_ERROR_WANT_READ)
                || (g_ms - t0) > NET_TIMEOUT_MS)
            return -1;
        net_poll();
    }
}

/* Read exactly len bytes (or up to len when partial != 0). */
static int tls_read(WOLFSSL *ssl, uint8_t *buf, int len, int partial)
{
    int got = 0, ret, err;
    uint32_t t0 = g_ms;
    while (got < len) {
        ret = wolfSSL_read(ssl, buf + got, len - got);
        if (ret > 0) {
            got += ret;
            t0 = g_ms;
            if (partial)
                break;
            continue;
        }
        err = wolfSSL_get_error(ssl, ret);
        if ((err != WOLFSSL_ERROR_WANT_READ && err != WOLFSSL_ERROR_WANT_WRITE)
                || (g_ms - t0) > NET_TIMEOUT_MS)
            return got > 0 ? got : -1;
        net_poll();
    }
    return got;
}

static void print_session(WOLFSSL *ssl, const struct tls_stats *st)
{
    const char *grp = wolfSSL_get_curve_name(ssl);
    puts_("  TLS 1.3 ");
    puts_(wolfSSL_get_cipher(ssl));
    puts_(", key exchange ");
    puts_(grp != NULL ? grp : "?");
    puts_("\n  handshake ");
    put_ms(st->hs_us);
    puts_(", peak heap ");
    putu(st->heap_peak);
    puts_(" B, peak stack ");
    putu(st->stack_peak);
    puts_(" B\n");
}

/* ------------------------------------------------------------------------ */
/* Handshake benchmark                                                      */
/* ------------------------------------------------------------------------ */
static void bench_groups(void)
{
    static const int groups[] = {
        WOLFSSL_X25519MLKEM768, WOLFSSL_SECP256R1MLKEM768,
        WOLFSSL_ML_KEM_768, WOLFSSL_ECC_X25519, WOLFSSL_ECC_SECP256R1
    };
    static const char *names[] = {
        "X25519MLKEM768", "SECP256R1MLKEM768", "ML_KEM_768",
        "X25519", "SECP256R1"
    };
    unsigned int g;
    int r;

    puts_("\n=== TLS 1.3 handshake benchmark (");
    putu(OTA_BENCH_ROUNDS);
    puts_(" rounds per group, ECDSA P-256 mutual auth) ===\n");
    for (g = 0; g < sizeof(groups) / sizeof(groups[0]); g++) {
        uint32_t sum = 0, mn = 0xFFFFFFFFu, mx = 0, heap = 0, stk = 0;
        int ok = 0;
        for (r = 0; r < OTA_BENCH_ROUNDS; r++) {
            WOLFSSL *ssl;
            int fd;
            struct tls_stats st;
            uint8_t rsp[8];
            if (tls_open(groups[g], &ssl, &fd, &st) != 0)
                continue;
            if (tls_write_all(ssl, "PING\n", 5) == 0)
                (void)tls_read(ssl, rsp, 5, 0);
            tls_close(ssl, fd);
            sum += st.hs_us;
            if (st.hs_us < mn) mn = st.hs_us;
            if (st.hs_us > mx) mx = st.hs_us;
            if (st.heap_peak > heap) heap = st.heap_peak;
            if (st.stack_peak > stk) stk = st.stack_peak;
            ok++;
        }
        puts_("BENCH ");
        puts_(names[g]);
        puts_(": ");
        if (ok == 0) {
            puts_("FAILED\n");
            continue;
        }
        puts_("avg ");
        put_ms(sum / (uint32_t)ok);
        puts_(" min ");
        put_ms(mn);
        puts_(" max ");
        put_ms(mx);
        puts_(" n=");
        putu((uint32_t)ok);
        puts_(" heap ");
        putu(heap);
        puts_(" B stack ");
        putu(stk);
        puts_(" B\n");
    }
    puts_("=== benchmark done ===\n");
}

/* ------------------------------------------------------------------------ */
/* OTA: request, download into UPDATE, trigger wolfBoot                     */
/* ------------------------------------------------------------------------ */
static uint8_t ota_buf[OTA_CHUNK] __attribute__((aligned(16)));

static void ota_check(void)
{
    WOLFSSL *ssl;
    int fd;
    struct tls_stats st;
    char req[24];
    uint8_t hdr[12];
    uint32_t magic, size, ver, got = 0, fill = 0, t0, ms;
    int n;

    puts_("\n--- OTA check: ");
    puts_(OTA_SERVER_IP);
    puts_(":");
    putu(OTA_SERVER_PORT);
    puts_(" ---\n");
    if (tls_open(OTA_GROUP, &ssl, &fd, &st) != 0)
        return;
    print_session(ssl, &st);

    memcpy(req, "OTA ", 4);
    n = 4;
    {
        char d[11];
        int i = 10;
        uint32_t v = g_version;
        d[i] = 0;
        do { d[--i] = (char)('0' + v % 10u); v /= 10u; } while (v != 0u);
        while (d[i] != 0)
            req[n++] = d[i++];
    }
    req[n++] = '\n';
    if (tls_write_all(ssl, req, n) != 0 || tls_read(ssl, hdr, 12, 0) != 12) {
        puts_("OTA: no response\n");
        tls_close(ssl, fd);
        return;
    }
    memcpy(&magic, hdr, 4);
    memcpy(&size, hdr + 4, 4);
    memcpy(&ver, hdr + 8, 4);
    if (magic != OTA_MAGIC || size == 0u) {
        puts_("OTA: no update offered\n");
        tls_close(ssl, fd);
        return;
    }
    puts_("OTA: server offers version ");
    putu(ver);
    puts_(", ");
    putu(size);
    puts_(" bytes\n");
    if (ver == g_version) {
        puts_("OTA: already running this version\n");
        tls_close(ssl, fd);
        return;
    }
    if (size > ota_update_max()) {
        puts_("OTA: image too large for the UPDATE partition\n");
        tls_close(ssl, fd);
        return;
    }

    t0 = g_ms;
    if (tls_write_all(ssl, "GO\n", 3) != 0 || ota_update_erase(size) != 0) {
        puts_("OTA: erase failed\n");
        tls_close(ssl, fd);
        return;
    }
    puts_("OTA: UPDATE partition erased in ");
    putu(g_ms - t0);
    puts_(" ms, downloading...\n");

    t0 = g_ms;
    while (got < size) {
        uint32_t want = size - got;
        if (want > OTA_CHUNK - fill)
            want = OTA_CHUNK - fill;
        n = tls_read(ssl, ota_buf + fill, (int)want, 1);
        if (n <= 0) {
            puts_("OTA: download interrupted\n");
            tls_close(ssl, fd);
            return;
        }
        fill += (uint32_t)n;
        if (fill == OTA_CHUNK) {
            if (ota_update_write(got + (uint32_t)n - fill, ota_buf, fill) != 0) {
                puts_("OTA: flash write failed\n");
                tls_close(ssl, fd);
                return;
            }
            fill = 0;
        }
        got += (uint32_t)n;
    }
    if (fill != 0u) {
        uint32_t padded = (fill + 15u) & ~15u;
        memset(ota_buf + fill, 0xFF, padded - fill);
        if (ota_update_write(got - fill, ota_buf, padded) != 0) {
            puts_("OTA: flash write failed\n");
            tls_close(ssl, fd);
            return;
        }
    }
    ms = g_ms - t0;
    tls_close(ssl, fd);
    puts_("OTA: received ");
    putu(got);
    puts_(" bytes in ");
    putu(ms);
    puts_(" ms (");
    putu(ms ? (got / ms) : 0u);
    puts_(" KB/s)\n");
    puts_("OTA: triggering wolfBoot update, rebooting\n");
    ota_update_trigger();
    delay_ms(50);
    NVIC_SystemReset();
}

/* ------------------------------------------------------------------------ */
/* Main                                                                     */
/* ------------------------------------------------------------------------ */
int main(void)
{
    int ret;
    uint32_t t0;
    int btn_prev = 0;
    int auto_check = 1;

    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();
    clock_init();
    SysTick->LOAD = (SYSCLK_HZ / 1000u) - 1u;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
        SysTick_CTRL_ENABLE_Msk;
    /* wolfBoot jumps here with interrupts masked (cpsid i in do_boot) */
    __enable_irq();
    DEMCR |= (1u << 24);
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;
    console_init();
    io_init();
    rng_init();

    g_version = ota_fw_version();
    puts_("\n\n========================================\n");
    puts_(" wolfBoot + wolfIP + wolfSSL  PQC OTA\n");
    puts_(" NUCLEO-C5A3ZG  firmware version ");
    putu(g_version);
    puts_("\n========================================\n");
    if (ota_boot_confirm())
        puts_("New firmware booted: confirmed with wolfBoot_success()\n");
    {
        uint32_t rv;
        if (ota_update_refused(&rv)) {
            puts_("wolfBoot REFUSED the staged update (version ");
            putu(rv);
            puts_("): failed verification or downgrade. Still running "
                "version ");
            putu(g_version);
            puts_("\n");
            auto_check = 0;
        }
    }

    wolfSSL_SetAllocators(trk_malloc, trk_free, trk_realloc);
    wolfSSL_Init();

    wolfIP_init_static(&IPStack);
    ret = eth_init();
    puts_("Ethernet: PHY link ");
    puts_((ret >= 0 && (ret & 0x100)) ? "up\n" : "down\n");

    net_poll();
    if (dhcp_client_init(IPStack) >= 0) {
        t0 = g_ms;
        while (!dhcp_bound(IPStack) && dhcp_client_is_running(IPStack) &&
                (g_ms - t0) < DHCP_TIMEOUT_MS)
            net_poll();
    }
    if (!dhcp_bound(IPStack))
        wolfIP_ipconfig_set(IPStack, atoip4(WOLFIP_IP), atoip4(WOLFIP_NETMASK),
            atoip4(WOLFIP_GW));
    {
        ip4 ip = 0, nm = 0, gw = 0;
        wolfIP_ipconfig_get(IPStack, &ip, &nm, &gw);
        puts_(dhcp_bound(IPStack) ? "DHCP address " : "Static address ");
        put_ip(ip);
        puts_("\n");
    }

    ret = tls_ctx_init();
    if (ret != 0) {
        puts_("TLS context init failed ");
        putu((uint32_t)-ret);
        puts_("\n");
    }

    t0 = g_ms;
    while ((g_ms - t0) < 1000u)
        net_poll();
#ifdef OTA_BENCH_AT_BOOT
    bench_groups();
#endif
    if (auto_check)
        ota_check();
    puts_("\nReady: B1 or 'u' = check for an update, 'b' = benchmark "
        "handshakes, 't' = install staged UPDATE image\n");

    for (;;) {
        int c = getc_nb();
        int btn = button_pressed();
        net_poll();
        led_pattern();
        if (c == 'u' || (btn && !btn_prev))
            ota_check();
        else if (c == 'b')
            bench_groups();
        else if (c == 't') {
            puts_("Installing the image staged in UPDATE (version ");
            putu(ota_update_version());
            puts_("), rebooting\n");
            ota_update_trigger();
            delay_ms(50);
            NVIC_SystemReset();
        }
        btn_prev = btn;
    }
    return 0;
}
