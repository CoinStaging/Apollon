/* netinfo.c -- generated by Trunnel v1.5.2.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "trunnel-impl.h"

#include "netinfo.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int netinfo_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || netinfo_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

netinfo_addr_t *
netinfo_addr_new(void)
{
  netinfo_addr_t *val = trunnel_calloc(1, sizeof(netinfo_addr_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
netinfo_addr_clear(netinfo_addr_t *obj)
{
  (void) obj;
}

void
netinfo_addr_free(netinfo_addr_t *obj)
{
  if (obj == NULL)
    return;
  netinfo_addr_clear(obj);
  trunnel_memwipe(obj, sizeof(netinfo_addr_t));
  trunnel_free_(obj);
}

uint8_t
netinfo_addr_get_addr_type(const netinfo_addr_t *inp)
{
  return inp->addr_type;
}
int
netinfo_addr_set_addr_type(netinfo_addr_t *inp, uint8_t val)
{
  inp->addr_type = val;
  return 0;
}
uint8_t
netinfo_addr_get_len(const netinfo_addr_t *inp)
{
  return inp->len;
}
int
netinfo_addr_set_len(netinfo_addr_t *inp, uint8_t val)
{
  inp->len = val;
  return 0;
}
uint32_t
netinfo_addr_get_addr_ipv4(const netinfo_addr_t *inp)
{
  return inp->addr_ipv4;
}
int
netinfo_addr_set_addr_ipv4(netinfo_addr_t *inp, uint32_t val)
{
  inp->addr_ipv4 = val;
  return 0;
}
size_t
netinfo_addr_getlen_addr_ipv6(const netinfo_addr_t *inp)
{
  (void)inp;  return 16;
}

uint8_t
netinfo_addr_get_addr_ipv6(netinfo_addr_t *inp, size_t xap)
{
  trunnel_assert(xap < 16);
  return inp->addr_ipv6[xap];
}

uint8_t
netinfo_addr_getconst_addr_ipv6(const netinfo_addr_t *inp, size_t xap)
{
  return netinfo_addr_get_addr_ipv6((netinfo_addr_t*)inp, xap);
}
int
netinfo_addr_set_addr_ipv6(netinfo_addr_t *inp, size_t xap, uint8_t elt)
{
  trunnel_assert(xap < 16);
  inp->addr_ipv6[xap] = elt;
  return 0;
}

uint8_t *
netinfo_addr_getarray_addr_ipv6(netinfo_addr_t *inp)
{
  return inp->addr_ipv6;
}
const uint8_t  *
netinfo_addr_getconstarray_addr_ipv6(const netinfo_addr_t *inp)
{
  return (const uint8_t  *)netinfo_addr_getarray_addr_ipv6((netinfo_addr_t*)inp);
}
const char *
netinfo_addr_check(const netinfo_addr_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  switch (obj->addr_type) {

    case NETINFO_ADDR_TYPE_IPV4:
      break;

    case NETINFO_ADDR_TYPE_IPV6:
      break;

    default:
      break;
  }
  return NULL;
}

ssize_t
netinfo_addr_encoded_len(const netinfo_addr_t *obj)
{
  ssize_t result = 0;

  if (NULL != netinfo_addr_check(obj))
     return -1;


  /* Length of u8 addr_type */
  result += 1;

  /* Length of u8 len */
  result += 1;
  switch (obj->addr_type) {

    case NETINFO_ADDR_TYPE_IPV4:

      /* Length of u32 addr_ipv4 */
      result += 4;
      break;

    case NETINFO_ADDR_TYPE_IPV6:

      /* Length of u8 addr_ipv6[16] */
      result += 16;
      break;

    default:
      break;
  }
  return result;
}
int
netinfo_addr_clear_errors(netinfo_addr_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
netinfo_addr_encode(uint8_t *output, const size_t avail, const netinfo_addr_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = netinfo_addr_encoded_len(obj);
#endif

  uint8_t *backptr_len = NULL;

  if (NULL != (msg = netinfo_addr_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 addr_type */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->addr_type));
  written += 1; ptr += 1;

  /* Encode u8 len */
  backptr_len = ptr;
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->len));
  written += 1; ptr += 1;
  {
    size_t written_before_union = written;

    /* Encode union addr[addr_type] */
    trunnel_assert(written <= avail);
    switch (obj->addr_type) {

      case NETINFO_ADDR_TYPE_IPV4:

        /* Encode u32 addr_ipv4 */
        trunnel_assert(written <= avail);
        if (avail - written < 4)
          goto truncated;
        trunnel_set_uint32(ptr, trunnel_htonl(obj->addr_ipv4));
        written += 4; ptr += 4;
        break;

      case NETINFO_ADDR_TYPE_IPV6:

        /* Encode u8 addr_ipv6[16] */
        trunnel_assert(written <= avail);
        if (avail - written < 16)
          goto truncated;
        memcpy(ptr, obj->addr_ipv6, 16);
        written += 16; ptr += 16;
        break;

      default:
        break;
    }
    /* Write the length field back to len */
    trunnel_assert(written >= written_before_union);
#if UINT8_MAX < SIZE_MAX
    if (written - written_before_union > UINT8_MAX)
      goto check_failed;
#endif
    trunnel_set_uint8(backptr_len, (written - written_before_union));
  }


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As netinfo_addr_parse(), but do not allocate the output object.
 */
static ssize_t
netinfo_addr_parse_into(netinfo_addr_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 addr_type */
  CHECK_REMAINING(1, truncated);
  obj->addr_type = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;

  /* Parse u8 len */
  CHECK_REMAINING(1, truncated);
  obj->len = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  {
    size_t remaining_after;
    CHECK_REMAINING(obj->len, truncated);
    remaining_after = remaining - obj->len;
    remaining = obj->len;

    /* Parse union addr[addr_type] */
    switch (obj->addr_type) {

      case NETINFO_ADDR_TYPE_IPV4:

        /* Parse u32 addr_ipv4 */
        CHECK_REMAINING(4, fail);
        obj->addr_ipv4 = trunnel_ntohl(trunnel_get_uint32(ptr));
        remaining -= 4; ptr += 4;
        break;

      case NETINFO_ADDR_TYPE_IPV6:

        /* Parse u8 addr_ipv6[16] */
        CHECK_REMAINING(16, fail);
        memcpy(obj->addr_ipv6, ptr, 16);
        remaining -= 16; ptr += 16;
        break;

      default:
        /* Skip to end of union */
        ptr += remaining; remaining = 0;
        break;
    }
    if (remaining != 0)
      goto fail;
    remaining = remaining_after;
  }
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
netinfo_addr_parse(netinfo_addr_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = netinfo_addr_new();
  if (NULL == *output)
    return -1;
  result = netinfo_addr_parse_into(*output, input, len_in);
  if (result < 0) {
    netinfo_addr_free(*output);
    *output = NULL;
  }
  return result;
}
netinfo_cell_t *
netinfo_cell_new(void)
{
  netinfo_cell_t *val = trunnel_calloc(1, sizeof(netinfo_cell_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
netinfo_cell_clear(netinfo_cell_t *obj)
{
  (void) obj;
  netinfo_addr_free(obj->other_addr);
  obj->other_addr = NULL;
  {

    unsigned xap;
    for (xap = 0; xap < TRUNNEL_DYNARRAY_LEN(&obj->my_addrs); ++xap) {
      netinfo_addr_free(TRUNNEL_DYNARRAY_GET(&obj->my_addrs, xap));
    }
  }
  TRUNNEL_DYNARRAY_WIPE(&obj->my_addrs);
  TRUNNEL_DYNARRAY_CLEAR(&obj->my_addrs);
}

void
netinfo_cell_free(netinfo_cell_t *obj)
{
  if (obj == NULL)
    return;
  netinfo_cell_clear(obj);
  trunnel_memwipe(obj, sizeof(netinfo_cell_t));
  trunnel_free_(obj);
}

uint32_t
netinfo_cell_get_timestamp(const netinfo_cell_t *inp)
{
  return inp->timestamp;
}
int
netinfo_cell_set_timestamp(netinfo_cell_t *inp, uint32_t val)
{
  inp->timestamp = val;
  return 0;
}
struct netinfo_addr_st *
netinfo_cell_get_other_addr(netinfo_cell_t *inp)
{
  return inp->other_addr;
}
const struct netinfo_addr_st *
netinfo_cell_getconst_other_addr(const netinfo_cell_t *inp)
{
  return netinfo_cell_get_other_addr((netinfo_cell_t*) inp);
}
int
netinfo_cell_set_other_addr(netinfo_cell_t *inp, struct netinfo_addr_st *val)
{
  if (inp->other_addr && inp->other_addr != val)
    netinfo_addr_free(inp->other_addr);
  return netinfo_cell_set0_other_addr(inp, val);
}
int
netinfo_cell_set0_other_addr(netinfo_cell_t *inp, struct netinfo_addr_st *val)
{
  inp->other_addr = val;
  return 0;
}
uint8_t
netinfo_cell_get_n_my_addrs(const netinfo_cell_t *inp)
{
  return inp->n_my_addrs;
}
int
netinfo_cell_set_n_my_addrs(netinfo_cell_t *inp, uint8_t val)
{
  inp->n_my_addrs = val;
  return 0;
}
size_t
netinfo_cell_getlen_my_addrs(const netinfo_cell_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->my_addrs);
}

struct netinfo_addr_st *
netinfo_cell_get_my_addrs(netinfo_cell_t *inp, size_t xap)
{
  return TRUNNEL_DYNARRAY_GET(&inp->my_addrs, xap);
}

 const struct netinfo_addr_st *
netinfo_cell_getconst_my_addrs(const netinfo_cell_t *inp, size_t xap)
{
  return netinfo_cell_get_my_addrs((netinfo_cell_t*)inp, xap);
}
int
netinfo_cell_set_my_addrs(netinfo_cell_t *inp, size_t xap, struct netinfo_addr_st * elt)
{
  netinfo_addr_t *oldval = TRUNNEL_DYNARRAY_GET(&inp->my_addrs, xap);
  if (oldval && oldval != elt)
    netinfo_addr_free(oldval);
  return netinfo_cell_set0_my_addrs(inp, xap, elt);
}
int
netinfo_cell_set0_my_addrs(netinfo_cell_t *inp, size_t xap, struct netinfo_addr_st * elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->my_addrs, xap, elt);
  return 0;
}
int
netinfo_cell_add_my_addrs(netinfo_cell_t *inp, struct netinfo_addr_st * elt)
{
#if SIZE_MAX >= UINT8_MAX
  if (inp->my_addrs.n_ == UINT8_MAX)
    goto trunnel_alloc_failed;
#endif
  TRUNNEL_DYNARRAY_ADD(struct netinfo_addr_st *, &inp->my_addrs, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

struct netinfo_addr_st * *
netinfo_cell_getarray_my_addrs(netinfo_cell_t *inp)
{
  return inp->my_addrs.elts_;
}
const struct netinfo_addr_st *  const  *
netinfo_cell_getconstarray_my_addrs(const netinfo_cell_t *inp)
{
  return (const struct netinfo_addr_st *  const  *)netinfo_cell_getarray_my_addrs((netinfo_cell_t*)inp);
}
int
netinfo_cell_setlen_my_addrs(netinfo_cell_t *inp, size_t newlen)
{
  struct netinfo_addr_st * *newptr;
#if UINT8_MAX < SIZE_MAX
  if (newlen > UINT8_MAX)
    goto trunnel_alloc_failed;
#endif
  newptr = trunnel_dynarray_setlen(&inp->my_addrs.allocated_,
                 &inp->my_addrs.n_, inp->my_addrs.elts_, newlen,
                 sizeof(inp->my_addrs.elts_[0]), (trunnel_free_fn_t) netinfo_addr_free,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->my_addrs.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
netinfo_cell_check(const netinfo_cell_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  {
    const char *msg;
    if (NULL != (msg = netinfo_addr_check(obj->other_addr)))
      return msg;
  }
  {
    const char *msg;

    unsigned xap;
    for (xap = 0; xap < TRUNNEL_DYNARRAY_LEN(&obj->my_addrs); ++xap) {
      if (NULL != (msg = netinfo_addr_check(TRUNNEL_DYNARRAY_GET(&obj->my_addrs, xap))))
        return msg;
    }
  }
  if (TRUNNEL_DYNARRAY_LEN(&obj->my_addrs) != obj->n_my_addrs)
    return "Length mismatch for my_addrs";
  return NULL;
}

ssize_t
netinfo_cell_encoded_len(const netinfo_cell_t *obj)
{
  ssize_t result = 0;

  if (NULL != netinfo_cell_check(obj))
     return -1;


  /* Length of u32 timestamp */
  result += 4;

  /* Length of struct netinfo_addr other_addr */
  result += netinfo_addr_encoded_len(obj->other_addr);

  /* Length of u8 n_my_addrs */
  result += 1;

  /* Length of struct netinfo_addr my_addrs[n_my_addrs] */
  {

    unsigned xap;
    for (xap = 0; xap < TRUNNEL_DYNARRAY_LEN(&obj->my_addrs); ++xap) {
      result += netinfo_addr_encoded_len(TRUNNEL_DYNARRAY_GET(&obj->my_addrs, xap));
    }
  }
  return result;
}
int
netinfo_cell_clear_errors(netinfo_cell_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
netinfo_cell_encode(uint8_t *output, const size_t avail, const netinfo_cell_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = netinfo_cell_encoded_len(obj);
#endif

  if (NULL != (msg = netinfo_cell_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u32 timestamp */
  trunnel_assert(written <= avail);
  if (avail - written < 4)
    goto truncated;
  trunnel_set_uint32(ptr, trunnel_htonl(obj->timestamp));
  written += 4; ptr += 4;

  /* Encode struct netinfo_addr other_addr */
  trunnel_assert(written <= avail);
  result = netinfo_addr_encode(ptr, avail - written, obj->other_addr);
  if (result < 0)
    goto fail; /* XXXXXXX !*/
  written += result; ptr += result;

  /* Encode u8 n_my_addrs */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->n_my_addrs));
  written += 1; ptr += 1;

  /* Encode struct netinfo_addr my_addrs[n_my_addrs] */
  {

    unsigned xap;
    for (xap = 0; xap < TRUNNEL_DYNARRAY_LEN(&obj->my_addrs); ++xap) {
      trunnel_assert(written <= avail);
      result = netinfo_addr_encode(ptr, avail - written, TRUNNEL_DYNARRAY_GET(&obj->my_addrs, xap));
      if (result < 0)
        goto fail; /* XXXXXXX !*/
      written += result; ptr += result;
    }
  }


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As netinfo_cell_parse(), but do not allocate the output object.
 */
static ssize_t
netinfo_cell_parse_into(netinfo_cell_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u32 timestamp */
  CHECK_REMAINING(4, truncated);
  obj->timestamp = trunnel_ntohl(trunnel_get_uint32(ptr));
  remaining -= 4; ptr += 4;

  /* Parse struct netinfo_addr other_addr */
  result = netinfo_addr_parse(&obj->other_addr, ptr, remaining);
  if (result < 0)
    goto relay_fail;
  trunnel_assert((size_t)result <= remaining);
  remaining -= result; ptr += result;

  /* Parse u8 n_my_addrs */
  CHECK_REMAINING(1, truncated);
  obj->n_my_addrs = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;

  /* Parse struct netinfo_addr my_addrs[n_my_addrs] */
  TRUNNEL_DYNARRAY_EXPAND(netinfo_addr_t *, &obj->my_addrs, obj->n_my_addrs, {});
  {
    netinfo_addr_t * elt;
    unsigned xap;
    for (xap = 0; xap < obj->n_my_addrs; ++xap) {
      result = netinfo_addr_parse(&elt, ptr, remaining);
      if (result < 0)
        goto relay_fail;
      trunnel_assert((size_t)result <= remaining);
      remaining -= result; ptr += result;
      TRUNNEL_DYNARRAY_ADD(netinfo_addr_t *, &obj->my_addrs, elt, {netinfo_addr_free(elt);});
    }
  }
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 relay_fail:
  trunnel_assert(result < 0);
  return result;
 trunnel_alloc_failed:
  return -1;
}

ssize_t
netinfo_cell_parse(netinfo_cell_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = netinfo_cell_new();
  if (NULL == *output)
    return -1;
  result = netinfo_cell_parse_into(*output, input, len_in);
  if (result < 0) {
    netinfo_cell_free(*output);
    *output = NULL;
  }
  return result;
}
