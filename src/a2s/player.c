#include <stdlib.h>
#include <string.h>
#include "ssq/a2s/player.h"
#include "ssq/buf.h"
#include "ssq/query.h"
#include "ssq/response.h"

#define A2S_PLAYER_PAYLOAD_LEN 5
#define A2S_PLAYER_PAYLOAD_LEN_WITH_CHALL (A2S_PLAYER_PAYLOAD_LEN + sizeof (int32_t))

#define A2S_HEADER_PLAYER 0x55
#define S2A_HEADER_PLAYER 0x44

#define A2S_PLAYER_NOCHALL 0xFFFFFFFF

const uint8_t g_a2s_player_payload[A2S_PLAYER_PAYLOAD_LEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, A2S_HEADER_PLAYER
};

static inline void ssq_player_payload_init(char payload[]) {
    memcpy(payload, g_a2s_player_payload, A2S_PLAYER_PAYLOAD_LEN);
}

static inline void ssq_player_payload_setchall(char payload[], const int32_t chall) {
    memcpy(payload + A2S_PLAYER_PAYLOAD_LEN, &chall, sizeof (chall));
}

A2S_PLAYER *ssq_player_deserialize(
    const char       response[],
    const size_t     response_len,
    uint8_t   *const player_count,
    SSQ_ERROR *const err
) {
    SSQ_BUF buf = ssq_buf_init(response, response_len);

    if (ssq_response_istruncated(response, response_len))
        ssq_buf_forward(&buf, sizeof (int32_t));

    const uint8_t response_header = ssq_buf_get_uint8(&buf);

    A2S_PLAYER *players = NULL;

    if (response_header == S2A_HEADER_PLAYER) {
        *player_count = ssq_buf_get_uint8(&buf);

        if (*player_count != 0) {
            players = calloc(*player_count, sizeof (*players));

            if (players != NULL) {
                for (uint8_t i = 0; i < *player_count; ++i) {
                    players[i].index    = ssq_buf_get_uint8(&buf);
                    players[i].name     = ssq_buf_get_string(&buf, &(players[i].name_len));
                    players[i].score    = ssq_buf_get_int32(&buf);
                    players[i].duration = ssq_buf_get_float(&buf);
                }
            } else {
                ssq_error_set_sys(err);
            }
        }
    } else {
        ssq_error_set(err, SSQ_ERR_BADRES, "Invalid A2S_PLAYER response header");
    }

    return players;
}

A2S_PLAYER *ssq_player(SSQ_QUERIER *const querier, uint8_t *const player_count) {
    char payload[A2S_PLAYER_PAYLOAD_LEN_WITH_CHALL];
    ssq_player_payload_init(payload);
    ssq_player_payload_setchall(payload, A2S_PLAYER_NOCHALL);

    size_t response_len;
    char  *response = ssq_query(querier, payload, A2S_PLAYER_PAYLOAD_LEN_WITH_CHALL, &response_len);

    while (ssq_ok(querier) && ssq_response_haschall(response, response_len)) {
        const int32_t query_chall = ssq_response_getchall(response, response_len);
        ssq_player_payload_setchall(payload, query_chall);

        free(response);
        response = ssq_query(querier, payload, A2S_PLAYER_PAYLOAD_LEN_WITH_CHALL, &response_len);
    }

    A2S_PLAYER *players = NULL;

    if (ssq_ok(querier)) {
        players = ssq_player_deserialize(response, response_len, player_count, &(querier->err));
        free(response);
    }

    return players;
}

void ssq_player_free(A2S_PLAYER players[], const uint8_t player_count) {
    for (uint8_t i = 0; i < player_count; ++i)
        free(players[i].name);

    free(players);
}
