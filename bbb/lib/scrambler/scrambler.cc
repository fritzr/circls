#include "scrambler.h"

#include "stdint.h"
#include "sys/time.h"

#include <cstring>
#include <cstdlib>

#define SCRAMBLER_SHIFT_INIT 0x96969696

namespace scrambler {

class ScramblerImpl
  : public Scrambler
{
protected:
  ~ScramblerImpl();

  /* In-place config settings.  */
  config conf;

public:
  ScramblerImpl (const config *conf_in=NULL);

  /* Override */
  err_t get_config (config *conf_out) const;
  seed_t rseed (void);
  seed_t get_seed (void) const;
}; // end class ScramblerImpl

}; // end namespace scrambler

using namespace scrambler;

/*************** config ****************/

/* Config default constructor.  */
config::config (unsigned char _seed_len, seed_t _seed)
  : seed(_seed), seed_len(_seed_len)
{
}

/* Config copy constructor.  */
config::config (const config *other)
  : seed(other ? other->seed : SCRAMBLER_DEFAULT_SEED),
  seed_len(other ? other->seed_len : SCRAMBLER_DEFAULT_LEN)
{
}

seed_t
config::seed_mask (void) const
{
  seed_t mask = 0;
  unsigned char len = seed_len;
  while (len)
  {
    mask <<= 1;
    mask |= 1;
    --len;
  }
  return mask;
}

/*********** ScramblerImpl **************/

ScramblerImpl::~ScramblerImpl()
{
}

ScramblerImpl::ScramblerImpl (const config *conf_in)
  : conf(conf_in)
{
}

err_t
ScramblerImpl::get_config (config *conf_out) const
{
  if (conf_out)
    memcpy (conf_out, &conf, sizeof(config));
  return ESCRAM_SUCCESS;
}

seed_t
ScramblerImpl::get_seed (void) const
{
  return conf.seed;
}

seed_t
ScramblerImpl::rseed (void)
{
  seed_t seed = random ();
  conf.seed = seed & conf.seed_mask ();
  return seed;
}

/************** Scrambler ***************/
static void init_parity (void);

Scrambler *
Scrambler::create (const config *conf_in)
{
  static bool initialized = false;
  if (!initialized)
  {
    init_parity ();
    struct timeval tv = {0};
    gettimeofday (&tv, NULL);
    srandom (tv.tv_usec);
    initialized = true;
  }
  return new ScramblerImpl (conf_in);
}

#define TO_T(x) x##_t
#define BITS_TYPE_(x) TO_T(uint##x)
#define BITS_TYPE(x) BITS_TYPE_(x)

/* PARITY_BITMAP_BITS - width of each entry in the parity_bitmap table.
 * This is configurable.
 * The larger this width, the fewer entries we need in the bitmap table itself,
 * so the more memory efficient the table will be.
 */
#define PARITY_BITMAP_BITS 64
#define BITSEL_BITS    6 /* log2(PARITY_BITMAP_BITS) */
#define BITSEL_MASK 0x3f /* '1' x BITSEL_BITS */

/* The type of each entry in the parity_bitmap table, according to
 * PARITY_BITMAP_BITS. */
typedef BITS_TYPE(PARITY_BITMAP_BITS) parity_bitmap_t;

/* MAX_PARITY_BITS - width of largest number we want to support looking up.
 * This is configurable.
 * For example when set to 32, we support looking up the parity of any
 * 32-bit number. The larger this is, the more space the parity table will
 * take up.  */
#define MAX_PARITY_BITS 8 /* configurable */

/* INDEX_BITS - the number of bits used for index into the parity_bitmap.
 * This is the bit width of the largest parity number,
 *   minus log2(the bit width of entries in the parity_bitmap).
 */
#define INDEX_BITS MAX_PARITY_BITS-PARITY_BITMAP_SEL

/* Configure the type of a number which we support taking the parity of,
 * which is set by our MAX_PARITY_BITS.  */
typedef BITS_TYPE(MAX_PARITY_BITS) max_parity_t;

/* The parity bitmap allows constant-time lookup of the parity of a number.
 * It is computed once when the scrambler library is initialized (the first
 * Scrambler instance is created).
 *
 * An n-bit number x is broken up into [index:bitsel]:
 *
 *    n-1  ...  7 6   5 4 3 2 1 0
 *   [     index    |    bitsel   ]
 *
 * Then the 6-bit value 'bitsel' selects a bit in parity_bitmap[index],
 * such that parity_bitmap[index] & (1 << bitsel) is the parity of x.
 */
#define PARITY_BITMAP_ENTRIES ((1u << MAX_PARITY_BITS) >> BITSEL_BITS)
static parity_bitmap_t parity_bitmap[PARITY_BITMAP_ENTRIES];

static unsigned char get_parity_memo (max_parity_t i)
{
 /* Here we keep parity_found with the same format as parity_bitmap, except
  * that the bit is only set once we have memoized the parity of x. This is to
  * facilitate the recursive memo algorithm for building the table here.
  * After initialization it is not needed. */
  static parity_bitmap_t parity_found[PARITY_BITMAP_ENTRIES];

  unsigned char parity;

  /* for i == 0 or i == 1, we are done */
  if ((i & ~1) == 0)
    return i & 1;

  /* bottom 6 bits select one of the 64 bits which contain the parity */
  unsigned char parity_selbit  = i & BITSEL_MASK;
  unsigned char parity_selmask = 1 << parity_selbit;
  max_parity_t index = i >> BITSEL_BITS;

  /* if we have already memoized the parity, return it */
  if (parity_found[index] & parity_selbit)
    return parity_bitmap[index] & parity_selmask;

  /* if we actually need to compute the parity,
   * just xor the last bit with the parity of the rest of the bits */
  parity = get_parity_memo (i >> 1) ^ (i & 1);
  parity_bitmap[index] |= parity << parity_selbit;
  parity_found[index]  |= parity_selmask;
  return parity;
}

static void init_parity (void)
{
  /* Force parity generation of every available number.
   * This way get_parity can stay inlined.
   * TODO for a more generic approach, just call get_parity_memo instead of
   * get_parity for each number you need the parity of, and soon enough
   * it will be cached appropriately. */
  const max_parity_t max = (1u << (MAX_PARITY_BITS)) - 1u;
  for (max_parity_t i = 0; i < max; i++)
    get_parity_memo (i);
}

/* Constant-time parity lookup using static memoized parity table.  */
static inline unsigned char get_parity (max_parity_t i)
{
  unsigned char bitsel = (i & BITSEL_MASK);
  return (parity_bitmap[i >> BITSEL_BITS] & (1 << bitsel)) >> bitsel;
}


/* The scramble pipeline is performed using a shift-register. The
 * scramble is deterministically produced based on the seed according to the
 * following:
 *
 *   For each bit in the input bit-stream,
 *      output_bit = input_bit ^ parity { shift_register & seed }
 *      shiftreg <<= 1
 *      shiftreg &= seed_mask
 *      shiftreg |= output_bit
 *
 * Thus the seed indicates a parity mask in the shift-register. That is, the
 * bits that are 1 in the seed define which bits from the shift-register are
 * summed with the incoming bit at each stage. This effectively performs a
 * pseudorandom scrambling, which can easily be unscrambled by following the
 * same algorithm.
 * . */
err_t
Scrambler::scramble (const void *__restrict in, void *__restrict out,
    unsigned long size)
{
  config conf;
  err_t err = get_config (&conf);
  if (is_err (err))
    return err;

  seed_t seed_mask = conf.seed_mask ();
  seed_t shiftreg = SCRAMBLER_SHIFT_INIT & seed_mask;
  seed_t seed = conf.seed;

  unsigned char in_bits;
  unsigned char out_bit;
  unsigned char out_bits = 0;

  unsigned const char *in_  = reinterpret_cast<const unsigned char *> (in);
  unsigned char *out_ = reinterpret_cast<unsigned char *> (out);
  const unsigned char *const end = out_ + size;

  while (out_ != end)
  {
    in_bits = *in_++;

    /* Unrolled loop for performance. We do 8 bits (one byte) at a time.
     * Unfortunately because of the bit-level shift register we cannot
     * condense these operations to any wider data width. */
    // 8:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 7:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 6:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 5:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 4:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 3:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 2:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;
    // 1:
    out_bit = (in_bits & 1) ^ get_parity (shiftreg & seed);
    out_bits = (out_bits << 1) | out_bit;
    shiftreg = ((shiftreg << 1) & seed_mask) | out_bit;
    in_bits >>= 1;

    *out_++ = out_bits;
  }

  return ESCRAM_SUCCESS;
}

err_t
Scrambler::unscramble (const void *__restrict in, void *__restrict out,
    unsigned long size)
{
  return ESCRAM_UNIMPL;
}

static const char *errors[] = {
  "success",          // ESCRAM_SUCCESS    =  0, /* No error */
  "system error",     // ESCRAM_SYS        = -1, /* System error, see errno.  */
  "bad shift length", // ESCRAM_BAD_LENGTH = -2, /* Bad shift-length.  */
  "bad seed",         // ESCRAM_BAD_SEED   = -3, /* Bad seed value.  */
  "unimplemented",    // ESCRAM_UNIMPL     = -4, /* Unimplemented operation. */
};

const char *
Scrambler::strerror (err_t err)
{
  int val = -1 * static_cast<int> (err); // err should be negative
  if (err >= 0 && err < sizeof(errors)/sizeof(errors[0]))
    return errors[static_cast<unsigned int>(val)];

  // else
  return "unknown error";
}

