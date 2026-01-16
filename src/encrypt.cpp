#include "encrypt.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <vector>
#include <cstring>


std::string base64_encode(const std::string& in)
{
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        unsigned char a = in[i];
        unsigned char b = (i + 1 < in.size()) ? in[i + 1] : 0;
        unsigned char c = (i + 2 < in.size()) ? in[i + 2] : 0;

        out += b64[(a >> 2) & 0x3F];
        out += b64[((a & 0x03) << 4) | ((b >> 4) & 0x0F)];
        out += (i + 1 < in.size()) ? b64[((b & 0x0F) << 2) | ((c >> 6) & 0x03)] : '=';
        out += (i + 2 < in.size()) ? b64[c & 0x3F] : '=';

        i += 3;
    }
    return out;
}

std::string base64_decode(const std::string& in)
{
    std::vector<unsigned char> out;
    size_t i = 0;
    while (i < in.size()) {
        unsigned char a = (in[i] == '=') ? 0 : (strchr(b64.c_str(), in[i]) - b64.c_str());
        unsigned char b = (i + 1 < in.size() && in[i + 1] != '=') ? (strchr(b64.c_str(), in[i + 1]) - b64.c_str()) : 0;
        unsigned char c = (i + 2 < in.size() && in[i + 2] != '=') ? (strchr(b64.c_str(), in[i + 2]) - b64.c_str()) : 0;
        unsigned char d = (i + 3 < in.size() && in[i + 3] != '=') ? (strchr(b64.c_str(), in[i + 3]) - b64.c_str()) : 0;

        out.push_back((a << 2) | (b >> 4));
        if (i + 1 < in.size() && in[i + 1] != '=') out.push_back((b << 4) | (c >> 2));
        if (i + 2 < in.size() && in[i + 2] != '=') out.push_back((c << 6) | d);

        i += 4;
    }
    return std::string(out.begin(), out.end());
}

std::string encryptData(const std::string& data, const std::string& password) {
    unsigned char key[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), nullptr, 0, 100000, EVP_sha256(), 32, key);

    unsigned char iv[16];
    if (RAND_bytes(iv, 16) != 1) return "";

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    int len, ciphertext_len;
    std::vector<unsigned char> ciphertext(data.size() + 16);

    if (!EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (unsigned char*)data.data(), data.size())) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len = len;

    if (!EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);
    EVP_CIPHER_CTX_free(ctx);

    std::string result;
    result.append((char*)iv, 16);
    result.append((char*)ciphertext.data(), ciphertext_len);
    return result;
}

std::string decryptData(const std::string& data, const std::string& password) {
    if (data.size() < 16) return "";
    std::string iv_str = data.substr(0, 16);
    std::string ciphertext = data.substr(16);

    unsigned char key[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), nullptr, 0, 100000, EVP_sha256(), 32, key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, (unsigned char*)iv_str.c_str())) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }

    int len, plaintext_len;
    std::vector<unsigned char> plaintext(ciphertext.size());

    if (!EVP_DecryptUpdate(ctx, plaintext.data(), &len, (unsigned char*)ciphertext.data(), ciphertext.size())) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    plaintext_len += len;
    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext_len);

    return std::string(plaintext.begin(), plaintext.end());
}