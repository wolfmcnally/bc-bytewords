//
//  bc-bytewords.c
//
//  Copyright © 2020 by Blockchain Commons, LLC
//  Licensed under the "BSD-2-Clause Plus Patent License"
//

#include "bc-bytewords.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "crc32.h"

static const char* bytewords = "ableacidalsoapexaquaarchatomauntawayaxisbackbaldbarnbeltbetabiasbluebodybragbrewbulbbuzzcalmcashcatschefcityclawcodecolacookcostcruxcurlcuspcyandarkdatadaysdelidicedietdoordowndrawdropdrumdulldutyeacheasyechoedgeepicevenexamexiteyesfactfairfernfigsfilmfishfizzflapflewfluxfoxyfreefrogfuelfundgalagamegeargemsgiftgirlglowgoodgraygrimgurugushgyrohalfhanghardhawkheathelphighhillholyhopehornhutsicedideaidleinchinkyintoirisironitemjadejazzjoinjoltjowljudojugsjumpjunkjurykeepkenokeptkeyskickkilnkingkitekiwiknoblamblavalazyleaflegsliarlistlimplionlogoloudloveluaulucklungmainmanymathmazememomenumeowmildmintmissmonknailnavyneednewsnextnoonnotenumbobeyoboeomitonyxopenovalowlspaidpartpeckplaypluspoempoolposepuffpumapurrquadquizraceramprealredorichroadrockroofrubyruinrunsrustsafesagascarsetssilkskewslotsoapsolosongstubsurfswantacotasktaxitenttiedtimetinytoiltombtoystriptunatwinuglyundouniturgeuservastveryvetovialvibeviewvisavoidvowswallwandwarmwaspwavewaxywebswhatwhenwhizwolfworkyankyawnyellyogayurtzapszestzinczonezoomzero";

static uint32_t bytewords_hash(const uint8_t* bytes, size_t len) {
    return htonl(crc32(bytes, len));
}

static bool decode_word(const char* in_word, size_t word_len, uint8_t* out_index) {
    static int16_t* array = NULL;
    const size_t dim = 26;

    // Since the first and last letters of each Byteword are unique,
    // we can use them as indexes into a two-dimensional lookup table.
    // This table is generated lazily.
    if(array == NULL) {
        const size_t array_len = dim * dim;
        array = malloc(array_len * sizeof(int16_t));
        for(size_t i = 0; i < array_len; i++) {
            array[i] = -1;
        }
        for(size_t i = 0; i < 256; i++) {
            const char* byteword = bytewords + i * 4;
            size_t x = byteword[0] - 'a';
            size_t y = byteword[3] - 'a';
            size_t offset = y * dim + x;
            array[offset] = i;
        }
    }

    // If the coordinates generated by the first and last letters are out of bounds,
    // or the lookup table contains -1 at the coordinates, then the word is not valid.
    int x = tolower(in_word[0]) - 'a';
    int y = tolower(in_word[word_len == 4 ? 3 : 1]) - 'a';
    if(!(0 <= x && x < dim && 0 <= y && y < dim)) {
        return false;
    }
    size_t offset = y * dim + x;
    int16_t value = array[offset];
    if(value == -1) {
        return false;
    }

    // If we're decoding a full four-letter word, verify that the two middle letters are correct.
    if(word_len == 4) {
        const char* byteword = bytewords + value * 4;
        int c1 = tolower(in_word[1]);
        int c2 = tolower(in_word[2]);
        if(c1 != byteword[1] || c2 != byteword[2]) {
            return false;
        }
    }

    // Successful decode.
    *out_index = value;
    return true;
}

static void get_word(uint8_t index, char* word) {
    memcpy(word, &bytewords[index * 4], 4);
    word[4] = '\0';
}

static void get_minimal_word(uint8_t index, char* word) {
    const char* p = &bytewords[index * 4];
    word[0] = *p;
    word[1] = *(p + 3);
    word[2] = '\0';
}

static char* encode(const uint8_t* in_buf, size_t in_len, char* out_buf, char separator) {
    const uint8_t* in_p = in_buf;
    char* out_p = out_buf;
    for(int i = 0; i < in_len; i++) {
        uint8_t index = *in_p++;
        get_word(index, out_p);
        if(i < in_len - 1) {
            out_p[4] = separator;
        }
        out_p += 5;
    }
    return out_p - 1;
}

static size_t add_crc(const uint8_t* in_buf, size_t in_len, uint8_t** in_with_crc_buf) {
    uint32_t crc = bytewords_hash(in_buf, in_len);
    size_t in_with_crc_len = in_len + sizeof(crc);
    *in_with_crc_buf = malloc(in_with_crc_len);
    memcpy(*in_with_crc_buf, in_buf, in_len);
    memcpy(*in_with_crc_buf + in_len, &crc, sizeof(crc));
    return in_with_crc_len;
}

static char* encode_with_separator(const uint8_t* in_buf, size_t in_len, char separator) {
    uint8_t* in_with_crc_buf;
    size_t in_with_crc_len = add_crc(in_buf, in_len, &in_with_crc_buf);
    size_t out_len = in_with_crc_len * 5;
    char* out_buf = malloc(out_len);
    encode(in_with_crc_buf, in_with_crc_len, out_buf, separator);
    free(in_with_crc_buf);
    return out_buf;
}

static char* encode_minimal(const uint8_t* in_buf, size_t in_len) {
    uint8_t* in_with_crc_buf;
    size_t in_with_crc_len = add_crc(in_buf, in_len, &in_with_crc_buf);
    size_t out_len = in_with_crc_len * 2 + 1;
    char* out_buf = malloc(out_len);

    const uint8_t* in_p = in_with_crc_buf;
    char* out_p = out_buf;
    for(int i = 0; i < in_with_crc_len; i++) {
        uint8_t index = *in_p++;
        get_minimal_word(index, out_p);
        out_p += 2;
    }
    *out_p = '\0';

    free(in_with_crc_buf);
    return out_buf;
}

char* bytewords_encode(bw_style style, const uint8_t* in_buf, size_t in_len) {
    switch(style) {
        case bw_standard:
            return encode_with_separator(in_buf, in_len, ' ');
        case bw_uri:
            return encode_with_separator(in_buf, in_len, '-');
        case bw_minimal:
            return encode_minimal(in_buf, in_len);
        default:
            assert(false);
    }
}

static bool decode(const char* in_string, uint8_t** out_buf, size_t* out_len, char separator, size_t word_len) {
    size_t in_string_len = strlen(in_string);
    size_t out_max_size = in_string_len / (word_len + 1) + 1;
    if(out_max_size < 5) return false;
    uint8_t* buf = malloc(out_max_size);

    const char* in_p = in_string;
    const char* end_p = in_p + in_string_len;
    uint8_t* out_p = buf;

    while(in_p != end_p && end_p - in_p >= word_len) {
        uint8_t index;
        if(!decode_word(in_p, word_len, &index)) {
            goto fail;
        }
        *out_p++ = index;
        in_p += word_len;
        if(separator != 0 && in_p != end_p && *in_p == separator) {
            in_p++;
        }
    }

    size_t buf_len = out_p - buf;
    if(buf_len < 5) {
        goto fail;
    }

    size_t body_len = buf_len - sizeof(uint32_t);
    uint8_t* buf_checksum = buf + body_len;
    uint32_t checksum = bytewords_hash(buf, body_len);
    if(memcmp(&checksum, buf_checksum, sizeof(uint32_t)) != 0) {
        goto fail;
    }

    *out_buf = buf;
    *out_len = body_len;
    return true;

    fail:
        free(buf);
        return false;
}

bool bytewords_decode(bw_style style, const char* in_string, uint8_t** out_buf, size_t* out_len) {
    switch(style) {
        case bw_standard:
            return decode(in_string, out_buf, out_len, ' ', 4);
        case bw_uri:
            return decode(in_string, out_buf, out_len, '-', 4);
        case bw_minimal:
            return decode(in_string, out_buf, out_len, 0, 2);
        default:
            assert(false);
    }
}
