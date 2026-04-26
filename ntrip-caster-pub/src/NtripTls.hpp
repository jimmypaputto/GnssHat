/*
 * Jimmy Paputto 2026
 *
 * Thin TLS wrapper for NTRIP connections.
 * Only compiled when NTRIP_CASTER_HAS_TLS is defined (CMake: -DNTRIP_TLS_SUPPORT=ON).
 * When SSL is not available, all operations are no-ops / return errors.
 */

#ifndef NTRIP_TLS_HPP_
#define NTRIP_TLS_HPP_

#include <cerrno>
#include <cstddef>
#include <string>

#ifdef NTRIP_CASTER_HAS_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

namespace JimmyPaputto
{

    class NtripTlsSocket
    {
    public:
        NtripTlsSocket() = default;
        ~NtripTlsSocket() { close(); }

        NtripTlsSocket(const NtripTlsSocket &) = delete;
        NtripTlsSocket &operator=(const NtripTlsSocket &) = delete;

        /// Wrap an existing connected TCP socket fd with TLS.
        /// Returns true on success.  On failure the fd is NOT closed.
        bool wrap(int fd, const std::string &hostname, bool verifyPeer = true)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            ctx_ = SSL_CTX_new(TLS_client_method());
            if (!ctx_)
                return false;

            if (verifyPeer)
            {
                SSL_CTX_set_default_verify_paths(ctx_);
                SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
            }
            else
            {
                SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
            }

            ssl_ = SSL_new(ctx_);
            if (!ssl_)
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                return false;
            }

            SSL_set_fd(ssl_, fd);
            SSL_set_tlsext_host_name(ssl_, hostname.c_str());

            if (SSL_connect(ssl_) <= 0)
            {
                SSL_free(ssl_);
                ssl_ = nullptr;
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                return false;
            }
            fd_ = fd;
            return true;
#else
            (void)fd;
            (void)hostname;
            (void)verifyPeer;
            return false;
#endif
        }

        ssize_t read(void *buf, size_t len)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (!ssl_)
                return -1;
            int n = SSL_read(ssl_, buf, static_cast<int>(len));
            if (n > 0)
                return static_cast<ssize_t>(n);

            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_ZERO_RETURN)
                return 0; // clean shutdown
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            {
                errno = EAGAIN;
                return -1;
            }
            return -1;
#else
            (void)buf;
            (void)len;
            return -1;
#endif
        }

        ssize_t write(const void *buf, size_t len)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (!ssl_)
                return -1;
            int n = SSL_write(ssl_, buf, static_cast<int>(len));
            return n > 0 ? static_cast<ssize_t>(n) : -1;
#else
            (void)buf;
            (void)len;
            return -1;
#endif
        }

        void close()
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (ssl_)
            {
                SSL_shutdown(ssl_);
                SSL_free(ssl_);
                ssl_ = nullptr;
            }
            if (ctx_)
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
            }
            fd_ = -1;
#endif
        }

        bool isActive() const
        {
#ifdef NTRIP_CASTER_HAS_TLS
            return ssl_ != nullptr;
#else
            return false;
#endif
        }

        static bool isAvailable()
        {
#ifdef NTRIP_CASTER_HAS_TLS
            return true;
#else
            return false;
#endif
        }

    private:
#ifdef NTRIP_CASTER_HAS_TLS
        SSL_CTX *ctx_ = nullptr;
        SSL *ssl_ = nullptr;
        int fd_ = -1;
#endif
    };

    /// Server-side TLS context — one per caster, wraps accepted client fds.
    class NtripTlsServerContext
    {
    public:
        NtripTlsServerContext() = default;
        ~NtripTlsServerContext() { destroy(); }

        NtripTlsServerContext(const NtripTlsServerContext &) = delete;
        NtripTlsServerContext &operator=(const NtripTlsServerContext &) = delete;

        /// Initialise with PEM certificate and private key file paths.
        bool init(const std::string &certFile, const std::string &keyFile)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            ctx_ = SSL_CTX_new(TLS_server_method());
            if (!ctx_)
                return false;

            if (SSL_CTX_use_certificate_file(ctx_, certFile.c_str(),
                                             SSL_FILETYPE_PEM) <= 0)
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                return false;
            }
            if (SSL_CTX_use_PrivateKey_file(ctx_, keyFile.c_str(),
                                            SSL_FILETYPE_PEM) <= 0)
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                return false;
            }
            if (!SSL_CTX_check_private_key(ctx_))
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
                return false;
            }
            return true;
#else
            (void)certFile;
            (void)keyFile;
            return false;
#endif
        }

        /// Wrap an accepted client fd with TLS (SSL_accept).
        /// Returns an opaque SSL* handle on success, nullptr on failure.
        /// The caller must pass this handle to read/write/closeSsl.
        void *accept(int fd)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (!ctx_)
                return nullptr;
            SSL *ssl = SSL_new(ctx_);
            if (!ssl)
                return nullptr;
            SSL_set_fd(ssl, fd);
            if (SSL_accept(ssl) <= 0)
            {
                SSL_free(ssl);
                return nullptr;
            }
            return ssl;
#else
            (void)fd;
            return nullptr;
#endif
        }

        static ssize_t read(void *handle, void *buf, size_t len)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (!handle)
                return -1;
            SSL *ssl = static_cast<SSL *>(handle);
            int n = SSL_read(ssl, buf, static_cast<int>(len));
            if (n > 0)
                return static_cast<ssize_t>(n);
            int err = SSL_get_error(ssl, n);
            if (err == SSL_ERROR_ZERO_RETURN)
                return 0;
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            {
                errno = EAGAIN;
                return -1;
            }
            return -1;
#else
            (void)handle;
            (void)buf;
            (void)len;
            return -1;
#endif
        }

        static ssize_t write(void *handle, const void *buf, size_t len)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (!handle)
                return -1;
            SSL *ssl = static_cast<SSL *>(handle);
            int n = SSL_write(ssl, buf, static_cast<int>(len));
            return n > 0 ? static_cast<ssize_t>(n) : -1;
#else
            (void)handle;
            (void)buf;
            (void)len;
            return -1;
#endif
        }

        static void closeSsl(void *handle)
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (handle)
            {
                SSL *ssl = static_cast<SSL *>(handle);
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
#else
            (void)handle;
#endif
        }

        bool isActive() const
        {
#ifdef NTRIP_CASTER_HAS_TLS
            return ctx_ != nullptr;
#else
            return false;
#endif
        }

        void destroy()
        {
#ifdef NTRIP_CASTER_HAS_TLS
            if (ctx_)
            {
                SSL_CTX_free(ctx_);
                ctx_ = nullptr;
            }
#endif
        }

    private:
#ifdef NTRIP_CASTER_HAS_TLS
        SSL_CTX *ctx_ = nullptr;
#endif
    };

}

#endif // NTRIP_TLS_HPP_
