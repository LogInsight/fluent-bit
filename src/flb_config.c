/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>

#include <fluent-bit/flb_macros.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_plugins.h>
#include <fluent-bit/flb_io_tls.h>

struct flb_config *flb_config_init()
{
    struct flb_config *config;

    __flb_config_verbose = FLB_FALSE;

    config = calloc(1, sizeof(struct flb_config));
    if (!config) {
        perror("malloc");
        return NULL;
    }

    config->flush     = FLB_CONFIG_FLUSH_SECS;
    config->init_time = time(NULL);

    mk_list_init(&config->collectors);
    mk_list_init(&config->inputs);
    mk_list_init(&config->outputs);

    /* Register plugins */
    flb_register_plugins(config);

    return config;
}

void flb_config_exit(struct flb_config *config)
{

}

void flb_config_verbose(int status)
{
    __flb_config_verbose = status;
}
