/*
    search routine generated by gen.
    skip=uf1, match=fwd (using fwdr), shift=inc
*/
#ifndef CHARTYPE
#define CHARTYPE unsigned char
#endif
#define MAXPAT 256

#ifndef TABTYPE
#define TABTYPE long
#endif
typedef TABTYPE Tab;

extern int __mark(int);

static struct {
    int patlen;
    CHARTYPE pat[MAXPAT];
    Tab delta[256];
} pat;

/* void prep(CHARTYPE *base, int m) */
/* { */
/*     CHARTYPE *skipc; */
/*     CHARTYPE *pe, *p; */
/*     int j; */
/*     Tab *d; */

/*     pat.patlen = m; */
/*     /\* if (m > MAXPAT) *\/ */
/*     /\*     abort(); *\/ */
/*     for (int i = 0; __mark(0) & (i < pat.patlen); ++i) { */
/*         pat.pat[i] = base[i]; */
/*     } */
/*     skipc = 0; */
/*     d = pat.delta; */
/*     for (j = 0; __mark(1) & (j < 256); j++) */
/*         d[j] = pat.patlen; */
/*     for (p = pat.pat, pe = p + m - 1; __mark(2) & (p < pe); p++) */
/*         d[*p] = pe - p; */
/*     d[*p] = 0; */
/*     skipc = (CHARTYPE *)p; */
/* } */

int exec(CHARTYPE *base, int n, CHARTYPE* patBase, int m) {
    /* prep(patBase, m); */
    pat.patlen = m;
    int nmatch = 0;
    CHARTYPE *e, *s;
    Tab *d0 = pat.delta;
    int k;
    CHARTYPE *p, *q;
    CHARTYPE *ep;
    int n1 = pat.patlen - 1;

    s = base + pat.patlen - 1;
    e = base + n;
    for (int i = 0; __mark(10) & (i < pat.patlen); ++i) {
        e[i] = pat.pat[pat.patlen - 1];
    }
    ep = pat.pat + pat.patlen - 1;
    while (__mark(11) & (s < e)) {
        k = d0[*s];
        while (__mark(12) & (k)) {
            k = d0[*(s += k)];
        }
        if (s >= e)
            break;
        /* for (p = pat.pat, q = s - n1; __mark(13) & (p < ep);) { */
        /*     if (*q++ != *p++) */
        /*         goto mismatch; */
        /* } */
        nmatch++;
    mismatch:
        s++;
    }
    return (nmatch);
}
