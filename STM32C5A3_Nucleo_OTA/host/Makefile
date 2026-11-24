# Host OTA server. WOLFSSL_PREFIX: a wolfSSL install with TLS 1.3 + ML-KEM
# (default configure options enable both).
WOLFSSL_PREFIX ?= /usr/local
CFLAGS ?= -O2 -Wall -Wextra

ota_server: ota_server.c
	$(CC) $(CFLAGS) -I$(WOLFSSL_PREFIX)/include $< -o $@ \
	    $(WOLFSSL_PREFIX)/lib/libwolfssl.a -lm -lpthread

clean:
	rm -f ota_server

.PHONY: clean
