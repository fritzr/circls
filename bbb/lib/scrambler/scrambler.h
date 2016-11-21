#ifndef _SCRAMBLER_H_
#define _SCRAMBLER_H_

/* Shift-register: bits to be shifted are set to 1. */
typedef unsigned int seed_t;

/* Scrambler library version. */
extern unsigned int SCRAMBLER_LIB_VERSION;

#define SCRAMBLER_DEFAULT_SEED 0x12
#define SCRAMBLER_DEFAULT_LEN  5

namespace scrambler
{

/* Scrambler error codes, returned by various functions.  */
enum err_t {
  ESCRAM_SUCCESS    =  0, /* No error */
  /* When an error is ESCRAM_SYS, check system errno for the real error.  */
  ESCRAM_SYS        = -1, /* System error, see errno.  */
  ESCRAM_BAD_LENGTH = -2, /* Bad shift-length.  */
  ESCRAM_BAD_SEED   = -3, /* Bad seed value.  */
  ESCRAM_UNIMPL     = -4, /* Unimplemented operation. */
};

struct config {
  /* The shiftregister seed MUST be the same on the scramble side as the
   * un-scramble side for accurate data reproduction. */
  seed_t seed;

  /* The seed length determines the number of bits used from the seed.
   * Smaller seed lengths are more performant, and higher shift-lengths
   * scramble the input more (which may or may not be "better").
   * The seed length should not exceed the bit width of seed_t. */
  unsigned char seed_len;

  /* Return a mask for the bottom seed_len bits of a seed.  */
  seed_t seed_mask (void) const;

  /* Default config settings.  */
  config (seed_t seed=SCRAMBLER_DEFAULT_SEED,
      unsigned char seed_len=SCRAMBLER_DEFAULT_LEN);

  /* Copy constructor.  */
  config (const config *other);
};

class Scrambler {
protected:
  virtual ~Scrambler() {}

  /* Protected default constructor.  */
  Scrambler() {}

public:
  /* Allocate and initialize a Scrambler which can be used to perform
   * the functions below. If the config given is NULL, use default options. */
  static Scrambler *create (const config *conf_in=0);

  /* Whether the err_t returned from a method is actually an error.  */
  static inline bool is_err (err_t err) {
    return static_cast<int>(err) < 0;
  }

  /* Get the scrambler config settings. */
  virtual err_t get_config (config *conf_out) const=0;

  /* Generate and return a random seed, storing it in this instance as well. */
  virtual seed_t rseed (void)=0;

  /* Scramble the given data into out. Both in and out should already have
   * size bytes allocated.
   * Returns 0 for success, negative on failure. */
  virtual err_t scramble (const void *__restrict in, void *__restrict out,
      unsigned long size);

  /* Scramble the given data and return a pointer to the scrambled data.
   * The returned buffer is freshly allocated with size bytes, and must be
   * freed by the caller.  */
  void *scramble_a (const void *in, unsigned long size) {
    unsigned char *out = new unsigned char[size];
    scramble (in, out, size);
    return out;
  }

  /* Unscramble the given data into out. Both in and out should already have
   * size bytes allocated.
   * Returns 0 for success, negative on failure.
   * Note that for accurate data recovery, the seed for the scrambler object
   * MUST be the same as when it was scrambled in the first place. */
  virtual err_t unscramble (const void *__restrict in, void *__restrict out,
      unsigned long size);

  /* Unscramble the given data and return a pointer to the unscrambled ata.
   * The returned buffer is freshly allocated with size bytes, and must be
   * freed by the caller.
   * Note that for accurate data recovery, the seed for the scrambler object
   * MUST be the same as when it was scrambled in the first place. */
  void *unscramble_a (const void *in, unsigned long size) {
    unsigned char *out = new unsigned char[size];
    unscramble (in, out, size);
    return out;
  }

  /* Return a string for describing the given scrambler error.
   * If the error is ESCRAM_SYS, the system strerror will be used on errno. */
  static const char *strerror (err_t err);

};

}; // end namespace scrambler

#endif // _SCRAMBLER_H_
