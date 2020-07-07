/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef HSDIR_APOLLON_ST_H
#define HSDIR_APOLLON_ST_H

/* Hidden service directory apollon used in a node_t which is set once we set
 * the consensus. */
struct hsdir_apollon_t {
  /* HSDir apollon to use when fetching a descriptor. */
  uint8_t fetch[DIGEST256_LEN];

  /* HSDir apollon used by services to store their first and second
   * descriptor. The first descriptor is chronologically older than the second
   * one and uses older TP and SRV values. */
  uint8_t store_first[DIGEST256_LEN];
  uint8_t store_second[DIGEST256_LEN];
};

#endif

