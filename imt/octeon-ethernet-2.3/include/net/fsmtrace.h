#ifndef __FSMTRACE_H__
#define __FSMTRACE_H__

#define FSMT_BUFSZ	64
#define FSMT_NDEVS	16
#define FSMT_NDEVS_MASK	0xF

typedef struct fsmt_record_s {
	int16_t state;
	int16_t rno;
	uint32_t tm;
	uint64_t d0;
	uint64_t d1;
	uint64_t d2;
} fsmt_record_t;

typedef struct fsmt_bdescr_s {
	int en;
	int it;
	int rno;
	fsmt_record_t buf[FSMT_BUFSZ];
} fsmt_bdescr_t;

extern fsmt_bdescr_t fsmtbds[FSMT_NDEVS];

#endif /*__FSMTRACE_H__*/
