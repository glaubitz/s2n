/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <sys/param.h>
#include <stdint.h>

#include "tls/extensions/s2n_client_server_name.h"
#include "tls/s2n_tls.h"
#include "tls/s2n_tls_parameters.h"

#include "utils/s2n_safety.h"

#define S2N_NAME_TYPE_HOST_NAME 0

static bool s2n_client_server_name_should_send(struct s2n_connection *conn);
static int s2n_client_server_name_send(struct s2n_connection *conn, struct s2n_stuffer *out);
static int s2n_client_server_name_recv(struct s2n_connection *conn, struct s2n_stuffer *extension);

const s2n_extension_type s2n_client_server_name_extension = {
    .iana_value = TLS_EXTENSION_SERVER_NAME,
    .is_response = false,
    .send = s2n_client_server_name_send,
    .recv = s2n_client_server_name_recv,
    .should_send = s2n_client_server_name_should_send,
    .if_missing = s2n_extension_noop_if_missing,
};

static bool s2n_client_server_name_should_send(struct s2n_connection *conn)
{
    return conn && strlen(conn->server_name) > 0;
}

static int s2n_client_server_name_send(struct s2n_connection *conn, struct s2n_stuffer *out)
{
    struct s2n_stuffer_reservation server_name_list_size = {0};
    GUARD(s2n_stuffer_reserve_uint16(out, &server_name_list_size));

    /* NameType, as described by RFC6066.
     * host_name is currently the only possible NameType defined. */
    GUARD(s2n_stuffer_write_uint8(out, S2N_NAME_TYPE_HOST_NAME));

    GUARD(s2n_stuffer_write_uint16(out, strlen(conn->server_name)));
    GUARD(s2n_stuffer_write_bytes(out, (const uint8_t *) conn->server_name, strlen(conn->server_name)));

    GUARD(s2n_stuffer_write_vector_size(&server_name_list_size));
    return S2N_SUCCESS;
}

static int s2n_client_server_name_check(struct s2n_connection *conn, struct s2n_stuffer *extension, uint16_t *server_name_len)
{
    notnull_check(conn);

    uint16_t size_of_all;
    GUARD(s2n_stuffer_read_uint16(extension, &size_of_all));
    lte_check(size_of_all, s2n_stuffer_data_available(extension));

    uint8_t server_name_type;
    GUARD(s2n_stuffer_read_uint8(extension, &server_name_type));
    eq_check(server_name_type, S2N_NAME_TYPE_HOST_NAME);

    GUARD(s2n_stuffer_read_uint16(extension, server_name_len));
    lt_check(*server_name_len, sizeof(conn->server_name));
    lte_check(*server_name_len, s2n_stuffer_data_available(extension));

    return S2N_SUCCESS;
}

static int s2n_client_server_name_recv(struct s2n_connection *conn, struct s2n_stuffer *extension)
{
    notnull_check(conn);

    /* Exit early if we've already parsed the server name */
    if (conn->server_name[0]) {
        return S2N_SUCCESS;
    }

    /* Ignore if malformed. We just won't use the server name. */
    uint16_t server_name_len;
    if (s2n_client_server_name_check(conn, extension, &server_name_len) != S2N_SUCCESS) {
        return S2N_SUCCESS;
    }

    uint8_t *server_name;
    notnull_check(server_name = s2n_stuffer_raw_read(extension, server_name_len));
    memcpy_check(conn->server_name, server_name, server_name_len);

    return S2N_SUCCESS;
}

int s2n_extensions_client_server_name_send(struct s2n_connection *conn, struct s2n_stuffer *out)
{
    return s2n_extension_send(&s2n_client_server_name_extension, conn, out);
}

int s2n_parse_client_hello_server_name(struct s2n_connection *conn, struct s2n_stuffer *extension)
{
    return s2n_extension_recv(&s2n_client_server_name_extension, conn, extension);
}
