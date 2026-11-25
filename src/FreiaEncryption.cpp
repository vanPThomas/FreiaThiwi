#include "FreiaEncryption.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <vector>
#include <cstring>


std::string FreiaEncryption::base64_encode(const std::string& in)
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

std::string FreiaEncryption::base64_decode(const std::string& in)
{
    if (in.empty() || in.size() % 4 != 0) return {};
    std::string out;
    out.reserve(in.size() / 4 * 3);
    const auto decode = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    for (size_t i = 0; i < in.size(); i += 4) {
        const int a = decode(in[i]);
        const int b = decode(in[i + 1]);
        const int c = in[i + 2] == '=' ? 0 : decode(in[i + 2]);
        const int d = in[i + 3] == '=' ? 0 : decode(in[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 ||
            (in[i + 2] == '=' && in[i + 3] != '=') ||
            ((in[i + 2] == '=' || in[i + 3] == '=') && i + 4 != in.size()))
            return {};
        out.push_back(static_cast<char>((a << 2) | (b >> 4)));
        if (in[i + 2] != '=')
            out.push_back(static_cast<char>((b << 4) | (c >> 2)));
        if (in[i + 3] != '=')
            out.push_back(static_cast<char>((c << 6) | d));
    }
    return out;
}

std::string FreiaEncryption::encryptData(const std::string& data, const Key& key) {
    // unsigned char key[32];
    // PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), nullptr, 0, 100000, EVP_sha256(), 32, key);

    unsigned char iv[16];
    if (RAND_bytes(iv, 16) != 1) return "";

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";

    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv)) {
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

std::string FreiaEncryption::decryptData(const std::string& data, const Key& key) {
    if (data.size() < 16) return "";
    std::string iv_str = data.substr(0, 16);
    std::string ciphertext = data.substr(16);

    // unsigned char key[32];
    // PKCS5_PBKDF2_HMAC(password.c_str(), password.size(), nullptr, 0, 100000, EVP_sha256(), 32, key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), (unsigned char*)iv_str.c_str())) {
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

FreiaEncryption::Key FreiaEncryption::deriveKey(const std::string& password)
{
    Key key{};
    if (!PKCS5_PBKDF2_HMAC(
            password.c_str(),
            password.size(),
            nullptr, 0,
            100000,
            EVP_sha256(),
            key.size(),
            key.data()))
    {}
    return key;
}