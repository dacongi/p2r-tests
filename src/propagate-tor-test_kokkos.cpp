/*
see README.txt for instructions
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <iostream>
#include <chrono>
#include <iomanip>

#include <Kokkos_Core.hpp>

#ifndef bsize
#define bsize 32
#endif
#ifndef ntrks
#define ntrks 8192
#endif

#define nb    (ntrks/bsize)

#ifndef nevts
#define nevts 100
#endif
#define smear 0.1

#ifndef NITER
#define NITER 5
#endif
#ifndef nlayer
#define nlayer 20
#endif

#ifndef elementsperthread 
#define elementsperthread 32
#endif

#ifndef threadsperblockx  
#define threadsperblockx 1
#endif

#ifndef num_streams
#define num_streams 1
#endif

KOKKOS_FUNCTION size_t PosInMtrx(size_t i, size_t j, size_t D) {
  return i*D+j;
}

size_t SymOffsets33(size_t i) {
  const size_t offs[9] = {0, 1, 3, 1, 2, 4, 3, 4, 5};
  return offs[i];
}

size_t SymOffsets66(size_t i) {
  const size_t offs[36] = {0, 1, 3, 6, 10, 15, 1, 2, 4, 7, 11, 16, 3, 4, 5, 8, 12, 17, 6, 7, 8, 9, 13, 18, 10, 11, 12, 13, 14, 19, 15, 16, 17, 18, 19, 20};
  return offs[i];
}

//using Kokkos::View<float*> =  Kokkos::View<float*>;
//using ViewVectorInt =  Kokkos::View<int*>;

struct ATRK {
  float par[6];
  float cov[21];
  int q;
  //  int hitidx[22];
};

struct AHIT {
  float pos[3];
  float cov[6];
};
struct MP1I {
  int data[1*bsize];
};

struct MP22I {
  int data[22*bsize];
};

struct MP1F {
  float data[1*bsize];
};

struct MP2F {
  float data[2*bsize];
};

struct MP3F {
  float data[3*bsize];
};

struct MP6F {
  float data[6*bsize];
};

struct MP3x3 {
  float data[9*bsize];
};
struct MP3x6 {
  float data[18*bsize];
};

struct MP2x2SF {
  float data[3*bsize];
};

struct MP3x3SF {
  float data[6*bsize];
};

struct MP6x6SF {
  float data[21*bsize];
};

struct MP6x6F {
  float data[36*bsize];
};

struct MPTRK {
  MP6F    par;
  MP6x6SF cov;
  MP1I    q;
  //  MP22I   hitidx;
};

struct MPHIT {
  MP3F    pos;
  MP3x3SF cov;
};

float randn(float mu, float sigma) {
  float U1, U2, W, mult;
  static float X1, X2;
  static int call = 0;
  if (call == 1) {
    call = !call;
    return (mu + sigma * (float) X2);
  } do {
    U1 = -1 + ((float) rand () / RAND_MAX) * 2;
    U2 = -1 + ((float) rand () / RAND_MAX) * 2;
    W = pow (U1, 2) + pow (U2, 2);
  }
  while (W >= 1 || W == 0); 
  mult = sqrt ((-2 * log (W)) / W);
  X1 = U1 * mult;
  X2 = U2 * mult; 
  call = !call; 
  return (mu + sigma * (float) X1);
}

KOKKOS_FUNCTION MPTRK* bTk(MPTRK* tracks, size_t ev, size_t ib) {
  return &(tracks[ib + nb*ev]);
}

KOKKOS_FUNCTION const MPTRK* bTk(const MPTRK* tracks, size_t ev, size_t ib) {
  return &(tracks[ib + nb*ev]);
}
KOKKOS_FUNCTION MPTRK* bTk(const Kokkos::View<MPTRK*> &tracks, size_t ev, size_t ib) {
  return &(tracks(ib + nb*ev));
}


KOKKOS_FUNCTION float q(const MP1I* bq, size_t it){
  return (*bq).data[it];
}
//
KOKKOS_FUNCTION float par(const MP6F* bpars, size_t it, size_t ipar){
  return (*bpars).data[it + ipar*bsize];
}
KOKKOS_FUNCTION float x    (const MP6F* bpars, size_t it){ return par(bpars, it, 0); }
KOKKOS_FUNCTION float y    (const MP6F* bpars, size_t it){ return par(bpars, it, 1); }
KOKKOS_FUNCTION float z    (const MP6F* bpars, size_t it){ return par(bpars, it, 2); }
KOKKOS_FUNCTION float ipt  (const MP6F* bpars, size_t it){ return par(bpars, it, 3); }
KOKKOS_FUNCTION float phi  (const MP6F* bpars, size_t it){ return par(bpars, it, 4); }
KOKKOS_FUNCTION float theta(const MP6F* bpars, size_t it){ return par(bpars, it, 5); }
//
KOKKOS_FUNCTION float par(const MPTRK* btracks, size_t it, size_t ipar){
  return par(&(*btracks).par,it,ipar);
}
KOKKOS_FUNCTION float x    (const MPTRK* btracks, size_t it){ return par(btracks, it, 0); }
KOKKOS_FUNCTION float y    (const MPTRK* btracks, size_t it){ return par(btracks, it, 1); }
KOKKOS_FUNCTION float z    (const MPTRK* btracks, size_t it){ return par(btracks, it, 2); }
KOKKOS_FUNCTION float ipt  (const MPTRK* btracks, size_t it){ return par(btracks, it, 3); }
KOKKOS_FUNCTION float phi  (const MPTRK* btracks, size_t it){ return par(btracks, it, 4); }
KOKKOS_FUNCTION float theta(const MPTRK* btracks, size_t it){ return par(btracks, it, 5); }
//
KOKKOS_FUNCTION float par(const MPTRK* tracks, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  const MPTRK* btracks = bTk(tracks, ev, ib);
  size_t it = tk % bsize;
  return par(btracks, it, ipar);
}
KOKKOS_FUNCTION float par(const Kokkos::View<MPTRK*> &tracks, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  const MPTRK* btracks = bTk(tracks, ev, ib);
  size_t it = tk % bsize;
  return par(btracks, it, ipar);
}
KOKKOS_FUNCTION float x    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 0); }
KOKKOS_FUNCTION float y    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 1); }
KOKKOS_FUNCTION float z    (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 2); }
KOKKOS_FUNCTION float ipt  (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 3); }
KOKKOS_FUNCTION float phi  (const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 4); }
KOKKOS_FUNCTION float theta(const MPTRK* tracks, size_t ev, size_t tk){ return par(tracks, ev, tk, 5); }
//
KOKKOS_FUNCTION void setpar(MP6F* bpars, size_t it, size_t ipar, float val){
  (*bpars).data[it + ipar*bsize] = val;
}
KOKKOS_FUNCTION void setx    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 0, val); }
KOKKOS_FUNCTION void sety    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 1, val); }
KOKKOS_FUNCTION void setz    (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 2, val); }
KOKKOS_FUNCTION void setipt  (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 3, val); }
KOKKOS_FUNCTION void setphi  (MP6F* bpars, size_t it, float val){ setpar(bpars, it, 4, val); }
KOKKOS_FUNCTION void settheta(MP6F* bpars, size_t it, float val){ setpar(bpars, it, 5, val); }
//
KOKKOS_FUNCTION void setpar(MPTRK* btracks, size_t it, size_t ipar, float val){
  setpar(&(*btracks).par,it,ipar,val);
}
KOKKOS_FUNCTION void setx    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 0, val); }
KOKKOS_FUNCTION void sety    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 1, val); }
KOKKOS_FUNCTION void setz    (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 2, val); }
KOKKOS_FUNCTION void setipt  (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 3, val); }
KOKKOS_FUNCTION void setphi  (MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 4, val); }
KOKKOS_FUNCTION void settheta(MPTRK* btracks, size_t it, float val){ setpar(btracks, it, 5, val); }

KOKKOS_FUNCTION const MPHIT* bHit(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t ib) {
  return &(hits(ib + nb*ev));
}
KOKKOS_FUNCTION const MPHIT* bHit(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t ib,size_t lay) {
return &(hits(lay + (ib*nlayer) +(ev*nlayer*nb)));
}
KOKKOS_FUNCTION const MPHIT* bHit(const MPHIT* hits, size_t ev, size_t ib) {
  return &(hits[ib + nb*ev]);
}

//
KOKKOS_FUNCTION float pos(const MP3F* hpos, size_t it, size_t ipar){
  return (*hpos).data[it + ipar*bsize];
}
KOKKOS_FUNCTION float x(const MP3F* hpos, size_t it)    { return pos(hpos, it, 0); }
KOKKOS_FUNCTION float y(const MP3F* hpos, size_t it)    { return pos(hpos, it, 1); }
KOKKOS_FUNCTION float z(const MP3F* hpos, size_t it)    { return pos(hpos, it, 2); }
//
KOKKOS_FUNCTION float pos(const MPHIT* hits, size_t it, size_t ipar){
  return pos(&(*hits).pos,it,ipar);
}
KOKKOS_FUNCTION float x(const MPHIT* hits, size_t it)    { return pos(hits, it, 0); }
KOKKOS_FUNCTION float y(const MPHIT* hits, size_t it)    { return pos(hits, it, 1); }
KOKKOS_FUNCTION float z(const MPHIT* hits, size_t it)    { return pos(hits, it, 2); }

KOKKOS_FUNCTION float pos(const MPHIT* hits, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  const MPHIT* bhits = bHit(hits, ev, ib);
  size_t it = tk % bsize;
  return pos(bhits,it,ipar);
}
KOKKOS_FUNCTION float x(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 0); }
KOKKOS_FUNCTION float y(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 1); }
KOKKOS_FUNCTION float z(const MPHIT* hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 2); }


KOKKOS_FUNCTION float pos(const Kokkos::View<MPHIT*> &hits, size_t it, size_t ipar){
  return pos(&(hits(0).pos),it,ipar);
}
KOKKOS_FUNCTION float x(const Kokkos::View<MPHIT*> &hits, size_t it)    { return pos(hits, it, 0); }
KOKKOS_FUNCTION float y(const Kokkos::View<MPHIT*> &hits, size_t it)    { return pos(hits, it, 1); }
KOKKOS_FUNCTION float z(const Kokkos::View<MPHIT*> &hits, size_t it)    { return pos(hits, it, 2); }
//
KOKKOS_FUNCTION float pos(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t tk, size_t ipar){
  size_t ib = tk/bsize;
  const MPHIT* bhits = bHit(hits, ev, ib);
  size_t it = tk % bsize;
  return pos(bhits,it,ipar);
}
KOKKOS_FUNCTION float x(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 0); }
KOKKOS_FUNCTION float y(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 1); }
KOKKOS_FUNCTION float z(const Kokkos::View<MPHIT*> &hits, size_t ev, size_t tk)    { return pos(hits, ev, tk, 2); }

Kokkos::View<MPTRK*>::HostMirror prepareTracks(ATRK inputtrk, Kokkos::View<MPTRK*> trk ) {

 // auto result = (MPTRK*) malloc(nevts*nb*sizeof(MPTRK)); //fixme, align?

  auto result = Kokkos::create_mirror_view( trk );

  printf("start prepare");
  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t ie=0;ie<nevts;++ie) {
    for (size_t ib=0;ib<nb;++ib) {
      for (size_t it=0;it<bsize;++it) {
	      //par
	      for (size_t ip=0;ip<6;++ip) {
	        result(ib + nb*ie).par.data[it + ip*bsize] = (1+smear*randn(0,1))*inputtrk.par[ip];
	      }
	      //cov
	      for (size_t ip=0;ip<21;++ip) {
	        result(ib + nb*ie).cov.data[it + ip*bsize] = (1+smear*randn(0,1))*inputtrk.cov[ip];
	      }
	      //q
	      result(ib + nb*ie).q.data[it] = inputtrk.q-2*ceil(-0.5 + (float)rand() / RAND_MAX);//fixme check
      }
    }
  }
  printf("end prepare");
  return result;
}

Kokkos::View<MPHIT*>::HostMirror prepareHits(AHIT inputhit, Kokkos::View<MPHIT*> hit) {
  
  auto result = Kokkos::create_mirror_view( hit );
  // store in element order for bunches of bsize matrices (a la matriplex)
  for (size_t lay=0;lay<nlayer;++lay) {
    for (size_t ie=0;ie<nevts;++ie) {
      for (size_t ib=0;ib<nb;++ib) {
        for (size_t it=0;it<bsize;++it) {
        	//pos
        	for (size_t ip=0;ip<3;++ip) {
        	  result(lay+nlayer*(ib + nb*ie)).pos.data[it + ip*bsize] = (1+smear*randn(0,1))*inputhit.pos[ip];
        	}
        	//cov
        	for (size_t ip=0;ip<6;++ip) {
        	  result(lay+nlayer*(ib + nb*ie)).cov.data[it + ip*bsize] = (1+smear*randn(0,1))*inputhit.cov[ip];
        	}
        }
      }
    }
  }
  return result;
}

#define N bsize
template<typename member_type>
KOKKOS_FUNCTION void MultHelixProp(const MP6x6F* A, const MP6x6SF* B, MP6x6F* C, const member_type& teamMember) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t n) 
  {
    c[ 0*N+n] = a[ 0*N+n]*b[ 0*N+n] + a[ 1*N+n]*b[ 1*N+n] + a[ 3*N+n]*b[ 6*N+n] + a[ 4*N+n]*b[10*N+n];
    c[ 1*N+n] = a[ 0*N+n]*b[ 1*N+n] + a[ 1*N+n]*b[ 2*N+n] + a[ 3*N+n]*b[ 7*N+n] + a[ 4*N+n]*b[11*N+n];
    c[ 2*N+n] = a[ 0*N+n]*b[ 3*N+n] + a[ 1*N+n]*b[ 4*N+n] + a[ 3*N+n]*b[ 8*N+n] + a[ 4*N+n]*b[12*N+n];
    c[ 3*N+n] = a[ 0*N+n]*b[ 6*N+n] + a[ 1*N+n]*b[ 7*N+n] + a[ 3*N+n]*b[ 9*N+n] + a[ 4*N+n]*b[13*N+n];
    c[ 4*N+n] = a[ 0*N+n]*b[10*N+n] + a[ 1*N+n]*b[11*N+n] + a[ 3*N+n]*b[13*N+n] + a[ 4*N+n]*b[14*N+n];
    c[ 5*N+n] = a[ 0*N+n]*b[15*N+n] + a[ 1*N+n]*b[16*N+n] + a[ 3*N+n]*b[18*N+n] + a[ 4*N+n]*b[19*N+n];
    c[ 6*N+n] = a[ 6*N+n]*b[ 0*N+n] + a[ 7*N+n]*b[ 1*N+n] + a[ 9*N+n]*b[ 6*N+n] + a[10*N+n]*b[10*N+n];
    c[ 7*N+n] = a[ 6*N+n]*b[ 1*N+n] + a[ 7*N+n]*b[ 2*N+n] + a[ 9*N+n]*b[ 7*N+n] + a[10*N+n]*b[11*N+n];
    c[ 8*N+n] = a[ 6*N+n]*b[ 3*N+n] + a[ 7*N+n]*b[ 4*N+n] + a[ 9*N+n]*b[ 8*N+n] + a[10*N+n]*b[12*N+n];
    c[ 9*N+n] = a[ 6*N+n]*b[ 6*N+n] + a[ 7*N+n]*b[ 7*N+n] + a[ 9*N+n]*b[ 9*N+n] + a[10*N+n]*b[13*N+n];
    c[10*N+n] = a[ 6*N+n]*b[10*N+n] + a[ 7*N+n]*b[11*N+n] + a[ 9*N+n]*b[13*N+n] + a[10*N+n]*b[14*N+n];
    c[11*N+n] = a[ 6*N+n]*b[15*N+n] + a[ 7*N+n]*b[16*N+n] + a[ 9*N+n]*b[18*N+n] + a[10*N+n]*b[19*N+n];
    c[12*N+n] = a[12*N+n]*b[ 0*N+n] + a[13*N+n]*b[ 1*N+n] + b[ 3*N+n] + a[15*N+n]*b[ 6*N+n] + a[16*N+n]*b[10*N+n] + a[17*N+n]*b[15*N+n];
    c[13*N+n] = a[12*N+n]*b[ 1*N+n] + a[13*N+n]*b[ 2*N+n] + b[ 4*N+n] + a[15*N+n]*b[ 7*N+n] + a[16*N+n]*b[11*N+n] + a[17*N+n]*b[16*N+n];
    c[14*N+n] = a[12*N+n]*b[ 3*N+n] + a[13*N+n]*b[ 4*N+n] + b[ 5*N+n] + a[15*N+n]*b[ 8*N+n] + a[16*N+n]*b[12*N+n] + a[17*N+n]*b[17*N+n];
    c[15*N+n] = a[12*N+n]*b[ 6*N+n] + a[13*N+n]*b[ 7*N+n] + b[ 8*N+n] + a[15*N+n]*b[ 9*N+n] + a[16*N+n]*b[13*N+n] + a[17*N+n]*b[18*N+n];
    c[16*N+n] = a[12*N+n]*b[10*N+n] + a[13*N+n]*b[11*N+n] + b[12*N+n] + a[15*N+n]*b[13*N+n] + a[16*N+n]*b[14*N+n] + a[17*N+n]*b[19*N+n];
    c[17*N+n] = a[12*N+n]*b[15*N+n] + a[13*N+n]*b[16*N+n] + b[17*N+n] + a[15*N+n]*b[18*N+n] + a[16*N+n]*b[19*N+n] + a[17*N+n]*b[20*N+n];
    c[18*N+n] = a[18*N+n]*b[ 0*N+n] + a[19*N+n]*b[ 1*N+n] + a[21*N+n]*b[ 6*N+n] + a[22*N+n]*b[10*N+n];
    c[19*N+n] = a[18*N+n]*b[ 1*N+n] + a[19*N+n]*b[ 2*N+n] + a[21*N+n]*b[ 7*N+n] + a[22*N+n]*b[11*N+n];
    c[20*N+n] = a[18*N+n]*b[ 3*N+n] + a[19*N+n]*b[ 4*N+n] + a[21*N+n]*b[ 8*N+n] + a[22*N+n]*b[12*N+n];
    c[21*N+n] = a[18*N+n]*b[ 6*N+n] + a[19*N+n]*b[ 7*N+n] + a[21*N+n]*b[ 9*N+n] + a[22*N+n]*b[13*N+n];
    c[22*N+n] = a[18*N+n]*b[10*N+n] + a[19*N+n]*b[11*N+n] + a[21*N+n]*b[13*N+n] + a[22*N+n]*b[14*N+n];
    c[23*N+n] = a[18*N+n]*b[15*N+n] + a[19*N+n]*b[16*N+n] + a[21*N+n]*b[18*N+n] + a[22*N+n]*b[19*N+n];
    c[24*N+n] = a[24*N+n]*b[ 0*N+n] + a[25*N+n]*b[ 1*N+n] + a[27*N+n]*b[ 6*N+n] + a[28*N+n]*b[10*N+n];
    c[25*N+n] = a[24*N+n]*b[ 1*N+n] + a[25*N+n]*b[ 2*N+n] + a[27*N+n]*b[ 7*N+n] + a[28*N+n]*b[11*N+n];
    c[26*N+n] = a[24*N+n]*b[ 3*N+n] + a[25*N+n]*b[ 4*N+n] + a[27*N+n]*b[ 8*N+n] + a[28*N+n]*b[12*N+n];
    c[27*N+n] = a[24*N+n]*b[ 6*N+n] + a[25*N+n]*b[ 7*N+n] + a[27*N+n]*b[ 9*N+n] + a[28*N+n]*b[13*N+n];
    c[28*N+n] = a[24*N+n]*b[10*N+n] + a[25*N+n]*b[11*N+n] + a[27*N+n]*b[13*N+n] + a[28*N+n]*b[14*N+n];
    c[29*N+n] = a[24*N+n]*b[15*N+n] + a[25*N+n]*b[16*N+n] + a[27*N+n]*b[18*N+n] + a[28*N+n]*b[19*N+n];
    c[30*N+n] = b[15*N+n];
    c[31*N+n] = b[16*N+n];
    c[32*N+n] = b[17*N+n];
    c[33*N+n] = b[18*N+n];
    c[34*N+n] = b[19*N+n];
    c[35*N+n] = b[20*N+n];
  });
}

template<typename member_type>
KOKKOS_FUNCTION void MultHelixPropTransp(const MP6x6F* A, const MP6x6F* B, MP6x6SF* C, const member_type& teamMember) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t n) 
  {
    c[ 0*N+n] = b[ 0*N+n]*a[ 0*N+n] + b[ 1*N+n]*a[ 1*N+n] + b[ 3*N+n]*a[ 3*N+n] + b[ 4*N+n]*a[ 4*N+n];
    c[ 1*N+n] = b[ 6*N+n]*a[ 0*N+n] + b[ 7*N+n]*a[ 1*N+n] + b[ 9*N+n]*a[ 3*N+n] + b[10*N+n]*a[ 4*N+n];
    c[ 2*N+n] = b[ 6*N+n]*a[ 6*N+n] + b[ 7*N+n]*a[ 7*N+n] + b[ 9*N+n]*a[ 9*N+n] + b[10*N+n]*a[10*N+n];
    c[ 3*N+n] = b[12*N+n]*a[ 0*N+n] + b[13*N+n]*a[ 1*N+n] + b[15*N+n]*a[ 3*N+n] + b[16*N+n]*a[ 4*N+n];
    c[ 4*N+n] = b[12*N+n]*a[ 6*N+n] + b[13*N+n]*a[ 7*N+n] + b[15*N+n]*a[ 9*N+n] + b[16*N+n]*a[10*N+n];
    c[ 5*N+n] = b[12*N+n]*a[12*N+n] + b[13*N+n]*a[13*N+n] + b[14*N+n] + b[15*N+n]*a[15*N+n] + b[16*N+n]*a[16*N+n] + b[17*N+n]*a[17*N+n];
    c[ 6*N+n] = b[18*N+n]*a[ 0*N+n] + b[19*N+n]*a[ 1*N+n] + b[21*N+n]*a[ 3*N+n] + b[22*N+n]*a[ 4*N+n];
    c[ 7*N+n] = b[18*N+n]*a[ 6*N+n] + b[19*N+n]*a[ 7*N+n] + b[21*N+n]*a[ 9*N+n] + b[22*N+n]*a[10*N+n];
    c[ 8*N+n] = b[18*N+n]*a[12*N+n] + b[19*N+n]*a[13*N+n] + b[20*N+n] + b[21*N+n]*a[15*N+n] + b[22*N+n]*a[16*N+n] + b[23*N+n]*a[17*N+n];
    c[ 9*N+n] = b[18*N+n]*a[18*N+n] + b[19*N+n]*a[19*N+n] + b[21*N+n]*a[21*N+n] + b[22*N+n]*a[22*N+n];
    c[10*N+n] = b[24*N+n]*a[ 0*N+n] + b[25*N+n]*a[ 1*N+n] + b[27*N+n]*a[ 3*N+n] + b[28*N+n]*a[ 4*N+n];
    c[11*N+n] = b[24*N+n]*a[ 6*N+n] + b[25*N+n]*a[ 7*N+n] + b[27*N+n]*a[ 9*N+n] + b[28*N+n]*a[10*N+n];
    c[12*N+n] = b[24*N+n]*a[12*N+n] + b[25*N+n]*a[13*N+n] + b[26*N+n] + b[27*N+n]*a[15*N+n] + b[28*N+n]*a[16*N+n] + b[29*N+n]*a[17*N+n];
    c[13*N+n] = b[24*N+n]*a[18*N+n] + b[25*N+n]*a[19*N+n] + b[27*N+n]*a[21*N+n] + b[28*N+n]*a[22*N+n];
    c[14*N+n] = b[24*N+n]*a[24*N+n] + b[25*N+n]*a[25*N+n] + b[27*N+n]*a[27*N+n] + b[28*N+n]*a[28*N+n];
    c[15*N+n] = b[30*N+n]*a[ 0*N+n] + b[31*N+n]*a[ 1*N+n] + b[33*N+n]*a[ 3*N+n] + b[34*N+n]*a[ 4*N+n];
    c[16*N+n] = b[30*N+n]*a[ 6*N+n] + b[31*N+n]*a[ 7*N+n] + b[33*N+n]*a[ 9*N+n] + b[34*N+n]*a[10*N+n];
    c[17*N+n] = b[30*N+n]*a[12*N+n] + b[31*N+n]*a[13*N+n] + b[32*N+n] + b[33*N+n]*a[15*N+n] + b[34*N+n]*a[16*N+n] + b[35*N+n]*a[17*N+n];
    c[18*N+n] = b[30*N+n]*a[18*N+n] + b[31*N+n]*a[19*N+n] + b[33*N+n]*a[21*N+n] + b[34*N+n]*a[22*N+n];
    c[19*N+n] = b[30*N+n]*a[24*N+n] + b[31*N+n]*a[25*N+n] + b[33*N+n]*a[27*N+n] + b[34*N+n]*a[28*N+n];
    c[20*N+n] = b[35*N+n];
  });
}


void KalmanGainInv(const MP6x6SF* A, const MP3x3SF* B, MP3x3* C) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  for (int n = 0; n < N; ++n)
  {
    double det =
      ((a[0*N+n]+b[0*N+n])*(((a[ 6*N+n]+b[ 3*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[7*N+n]+b[4*N+n])))) -
      ((a[1*N+n]+b[1*N+n])*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[2*N+n]+b[2*N+n])))) +
      ((a[2*N+n]+b[2*N+n])*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[6*N+n]+b[3*N+n]))));
    double invdet = 1.0/det;

    c[ 0*N+n] =  invdet*(((a[ 6*N+n]+b[ 3*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 1*N+n] =  -1*invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 2*N+n] =  invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[7*N+n]+b[4*N+n])));
    c[ 3*N+n] =  -1*invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[7*N+n]+b[4*N+n]) *(a[2*N+n]+b[2*N+n])));
    c[ 4*N+n] =  invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[11*N+n]+b[5*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[2*N+n]+b[2*N+n])));
    c[ 5*N+n] =  -1*invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[1*N+n]+b[1*N+n])));
    c[ 6*N+n] =  invdet*(((a[ 1*N+n]+b[ 1*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[6*N+n]+b[3*N+n])));
    c[ 7*N+n] =  -1*invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[7*N+n]+b[4*N+n])) - ((a[2*N+n]+b[2*N+n]) *(a[1*N+n]+b[1*N+n])));
    c[ 8*N+n] =  invdet*(((a[ 0*N+n]+b[ 0*N+n]) *(a[6*N+n]+b[3*N+n])) - ((a[1*N+n]+b[1*N+n]) *(a[1*N+n]+b[1*N+n])));
  }
}
void KalmanGain(const MP6x6SF* A, const MP3x3* B, MP3x6* C) {
  const float* a = (*A).data; //ASSUME_ALIGNED(a, 64);
  const float* b = (*B).data; //ASSUME_ALIGNED(b, 64);
  float* c = (*C).data;       //ASSUME_ALIGNED(c, 64);
  for (int n = 0; n < N; ++n)
  {
    c[ 0*N+n] = a[0*N+n]*b[0*N+n] + a[1*N+n]*b[3*N+n] + a[2*N+n]*b[6*N+n];
    c[ 1*N+n] = a[0*N+n]*b[1*N+n] + a[1*N+n]*b[4*N+n] + a[2*N+n]*b[7*N+n];
    c[ 2*N+n] = a[0*N+n]*b[2*N+n] + a[1*N+n]*b[5*N+n] + a[2*N+n]*b[8*N+n];
    c[ 3*N+n] = a[1*N+n]*b[0*N+n] + a[6*N+n]*b[3*N+n] + a[7*N+n]*b[6*N+n];
    c[ 4*N+n] = a[1*N+n]*b[1*N+n] + a[6*N+n]*b[4*N+n] + a[7*N+n]*b[7*N+n];
    c[ 5*N+n] = a[1*N+n]*b[2*N+n] + a[6*N+n]*b[5*N+n] + a[7*N+n]*b[8*N+n];
    c[ 6*N+n] = a[2*N+n]*b[0*N+n] + a[7*N+n]*b[3*N+n] + a[11*N+n]*b[6*N+n];
    c[ 7*N+n] = a[2*N+n]*b[1*N+n] + a[7*N+n]*b[4*N+n] + a[11*N+n]*b[7*N+n];
    c[ 8*N+n] = a[2*N+n]*b[2*N+n] + a[7*N+n]*b[5*N+n] + a[11*N+n]*b[8*N+n];
    c[ 9*N+n] = a[3*N+n]*b[0*N+n] + a[8*N+n]*b[3*N+n] + a[12*N+n]*b[6*N+n];
    c[ 10*N+n] = a[3*N+n]*b[1*N+n] + a[8*N+n]*b[4*N+n] + a[12*N+n]*b[7*N+n];
    c[ 11*N+n] = a[3*N+n]*b[2*N+n] + a[8*N+n]*b[5*N+n] + a[12*N+n]*b[8*N+n];
    c[ 12*N+n] = a[4*N+n]*b[0*N+n] + a[9*N+n]*b[3*N+n] + a[13*N+n]*b[6*N+n];
    c[ 13*N+n] = a[4*N+n]*b[1*N+n] + a[9*N+n]*b[4*N+n] + a[13*N+n]*b[7*N+n];
    c[ 14*N+n] = a[4*N+n]*b[2*N+n] + a[9*N+n]*b[5*N+n] + a[13*N+n]*b[8*N+n];
    c[ 15*N+n] = a[5*N+n]*b[0*N+n] + a[10*N+n]*b[3*N+n] + a[14*N+n]*b[6*N+n];
    c[ 16*N+n] = a[5*N+n]*b[1*N+n] + a[10*N+n]*b[4*N+n] + a[14*N+n]*b[7*N+n];
    c[ 17*N+n] = a[5*N+n]*b[2*N+n] + a[10*N+n]*b[5*N+n] + a[14*N+n]*b[8*N+n];
  }
}

KOKKOS_FUNCTION inline float hipo(float x, float y)
{
  return std::sqrt(x*x + y*y);
}

template <typename member_type>
KOKKOS_FUNCTION void KalmanUpdate(MP6x6SF* trkErr, MP6F* inPar, const MP3x3SF* hitErr, const MP3F* msP,
                                MP1F *rotT00, MP1F *rotT01, MP2x2SF *resErr_loc, MP3x6 *kGain,
                                MP2F *res_loc, MP6x6SF * newErr, const member_type& teamMember){
  
  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it){ 
    const float r = hipo(x(msP,it), y(msP,it));
    rotT00->data[it] = -(y(msP,it) + y(inPar,it)) / (2*r);
    rotT01->data[it] =  (x(msP,it) + x(inPar,it)) / (2*r);    
    
    resErr_loc->data[ 0*bsize+it] = (rotT00->data[it]*(trkErr->data[0*bsize+it] + hitErr->data[0*bsize+it]) +
                                    rotT01->data[it]*(trkErr->data[1*bsize+it] + hitErr->data[1*bsize+it]))*rotT00->data[it] +
                                   (rotT00->data[it]*(trkErr->data[1*bsize+it] + hitErr->data[1*bsize+it]) +
                                    rotT01->data[it]*(trkErr->data[2*bsize+it] + hitErr->data[2*bsize+it]))*rotT01->data[it];
    resErr_loc->data[ 1*bsize+it] = (trkErr->data[3*bsize+it] + hitErr->data[3*bsize+it])*rotT00->data[it] +
                                   (trkErr->data[4*bsize+it] + hitErr->data[4*bsize+it])*rotT01->data[it];
    resErr_loc->data[ 2*bsize+it] = (trkErr->data[5*bsize+it] + hitErr->data[5*bsize+it]);
  });

  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it) 
  {
    const double det = (double)resErr_loc->data[0*bsize+it] * resErr_loc->data[2*bsize+it] -
                       (double)resErr_loc->data[1*bsize+it] * resErr_loc->data[1*bsize+it];
    const float s   = 1.f / det;
    const float tmp = s * resErr_loc->data[2*bsize+it];
    resErr_loc->data[1*bsize+it] *= -s;
    resErr_loc->data[2*bsize+it]  = s * resErr_loc->data[0*bsize+it];
    resErr_loc->data[0*bsize+it]  = tmp;
  });

  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it)
   {
      kGain->data[ 0*bsize+it] = trkErr->data[ 0*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 1*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 3*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[ 1*bsize+it] = trkErr->data[ 0*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 1*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 3*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[ 2*bsize+it] = 0;
      kGain->data[ 3*bsize+it] = trkErr->data[ 1*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 2*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 4*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[ 4*bsize+it] = trkErr->data[ 1*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 2*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 4*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[ 5*bsize+it] = 0;
      kGain->data[ 6*bsize+it] = trkErr->data[ 3*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 4*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 5*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[ 7*bsize+it] = trkErr->data[ 3*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 4*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 5*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[ 8*bsize+it] = 0;
      kGain->data[ 9*bsize+it] = trkErr->data[ 6*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 7*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[ 8*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[10*bsize+it] = trkErr->data[ 6*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 7*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[ 8*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[11*bsize+it] = 0;
      kGain->data[12*bsize+it] = trkErr->data[10*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[11*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[12*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[13*bsize+it] = trkErr->data[10*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[11*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[12*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[14*bsize+it] = 0;
      kGain->data[15*bsize+it] = trkErr->data[15*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[16*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 0*bsize+it]) +
	                        trkErr->data[17*bsize+it]*resErr_loc->data[ 1*bsize+it];
      kGain->data[16*bsize+it] = trkErr->data[15*bsize+it]*(rotT00->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[16*bsize+it]*(rotT01->data[it]*resErr_loc->data[ 1*bsize+it]) +
	                        trkErr->data[17*bsize+it]*resErr_loc->data[ 2*bsize+it];
      kGain->data[17*bsize+it] = 0;
   });

  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it)
   {
     res_loc->data[0*bsize+it] =  rotT00->data[it]*(x(msP,it) - x(inPar,it)) + rotT01->data[it]*(y(msP,it) - y(inPar,it));
     res_loc->data[1*bsize+it] =  z(msP,it) - z(inPar,it);

     setx(inPar, it, x(inPar, it) + kGain->data[ 0*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[ 1*bsize+it] * res_loc->data[ 1*bsize+it]);
     sety(inPar, it, y(inPar, it) + kGain->data[ 3*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[ 4*bsize+it] * res_loc->data[ 1*bsize+it]);
     setz(inPar, it, z(inPar, it) + kGain->data[ 6*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[ 7*bsize+it] * res_loc->data[ 1*bsize+it]);
     setipt(inPar, it, ipt(inPar, it) + kGain->data[ 9*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[10*bsize+it] * res_loc->data[ 1*bsize+it]);
     setphi(inPar, it, phi(inPar, it) + kGain->data[12*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[13*bsize+it] * res_loc->data[ 1*bsize+it]);
     settheta(inPar, it, theta(inPar, it) + kGain->data[15*bsize+it] * res_loc->data[ 0*bsize+it] + kGain->data[16*bsize+it] * res_loc->data[ 1*bsize+it]);
   });
  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it)
   {
     newErr->data[ 0*bsize+it] = kGain->data[ 0*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[ 0*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 1*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[ 1*bsize+it] = kGain->data[ 3*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[ 3*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 4*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[ 2*bsize+it] = kGain->data[ 3*bsize+it]*rotT00->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 3*bsize+it]*rotT01->data[it]*trkErr->data[ 2*bsize+it] +
                                kGain->data[ 4*bsize+it]*trkErr->data[ 4*bsize+it];
     newErr->data[ 3*bsize+it] = kGain->data[ 6*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[ 6*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 7*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[ 4*bsize+it] = kGain->data[ 6*bsize+it]*rotT00->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 6*bsize+it]*rotT01->data[it]*trkErr->data[ 2*bsize+it] +
                                kGain->data[ 7*bsize+it]*trkErr->data[ 4*bsize+it];
     newErr->data[ 5*bsize+it] = kGain->data[ 6*bsize+it]*rotT00->data[it]*trkErr->data[ 3*bsize+it] +
                                kGain->data[ 6*bsize+it]*rotT01->data[it]*trkErr->data[ 4*bsize+it] +
                                kGain->data[ 7*bsize+it]*trkErr->data[ 5*bsize+it];
     newErr->data[ 6*bsize+it] = kGain->data[ 9*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[ 9*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[10*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[ 7*bsize+it] = kGain->data[ 9*bsize+it]*rotT00->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[ 9*bsize+it]*rotT01->data[it]*trkErr->data[ 2*bsize+it] +
                                kGain->data[10*bsize+it]*trkErr->data[ 4*bsize+it];
     newErr->data[ 8*bsize+it] = kGain->data[ 9*bsize+it]*rotT00->data[it]*trkErr->data[ 3*bsize+it] +
                                kGain->data[ 9*bsize+it]*rotT01->data[it]*trkErr->data[ 4*bsize+it] +
                                kGain->data[10*bsize+it]*trkErr->data[ 5*bsize+it];
     newErr->data[ 9*bsize+it] = kGain->data[ 9*bsize+it]*rotT00->data[it]*trkErr->data[ 6*bsize+it] +
                                kGain->data[ 9*bsize+it]*rotT01->data[it]*trkErr->data[ 7*bsize+it] +
                                kGain->data[10*bsize+it]*trkErr->data[ 8*bsize+it];
     newErr->data[10*bsize+it] = kGain->data[12*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[12*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[13*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[11*bsize+it] = kGain->data[12*bsize+it]*rotT00->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[12*bsize+it]*rotT01->data[it]*trkErr->data[ 2*bsize+it] +
                                kGain->data[13*bsize+it]*trkErr->data[ 4*bsize+it];
     newErr->data[12*bsize+it] = kGain->data[12*bsize+it]*rotT00->data[it]*trkErr->data[ 3*bsize+it] +
                                kGain->data[12*bsize+it]*rotT01->data[it]*trkErr->data[ 4*bsize+it] +
                                kGain->data[13*bsize+it]*trkErr->data[ 5*bsize+it];
     newErr->data[13*bsize+it] = kGain->data[12*bsize+it]*rotT00->data[it]*trkErr->data[ 6*bsize+it] +
                                kGain->data[12*bsize+it]*rotT01->data[it]*trkErr->data[ 7*bsize+it] +
                                kGain->data[13*bsize+it]*trkErr->data[ 8*bsize+it];
     newErr->data[14*bsize+it] = kGain->data[12*bsize+it]*rotT00->data[it]*trkErr->data[10*bsize+it] +
                                kGain->data[12*bsize+it]*rotT01->data[it]*trkErr->data[11*bsize+it] +
                                kGain->data[13*bsize+it]*trkErr->data[12*bsize+it];
     newErr->data[15*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[ 0*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[ 3*bsize+it];
     newErr->data[16*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[ 1*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[ 2*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[ 4*bsize+it];
     newErr->data[17*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[ 3*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[ 4*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[ 5*bsize+it];
     newErr->data[18*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[ 6*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[ 7*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[ 8*bsize+it];
     newErr->data[19*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[10*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[11*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[12*bsize+it];
     newErr->data[20*bsize+it] = kGain->data[15*bsize+it]*rotT00->data[it]*trkErr->data[15*bsize+it] +
                                kGain->data[15*bsize+it]*rotT01->data[it]*trkErr->data[16*bsize+it] +
                                kGain->data[16*bsize+it]*trkErr->data[17*bsize+it];

     newErr->data[ 0*bsize+it] = trkErr->data[ 0*bsize+it] - newErr->data[ 0*bsize+it];
     newErr->data[ 1*bsize+it] = trkErr->data[ 1*bsize+it] - newErr->data[ 1*bsize+it];
     newErr->data[ 2*bsize+it] = trkErr->data[ 2*bsize+it] - newErr->data[ 2*bsize+it];
     newErr->data[ 3*bsize+it] = trkErr->data[ 3*bsize+it] - newErr->data[ 3*bsize+it];
     newErr->data[ 4*bsize+it] = trkErr->data[ 4*bsize+it] - newErr->data[ 4*bsize+it];
     newErr->data[ 5*bsize+it] = trkErr->data[ 5*bsize+it] - newErr->data[ 5*bsize+it];
     newErr->data[ 6*bsize+it] = trkErr->data[ 6*bsize+it] - newErr->data[ 6*bsize+it];
     newErr->data[ 7*bsize+it] = trkErr->data[ 7*bsize+it] - newErr->data[ 7*bsize+it];
     newErr->data[ 8*bsize+it] = trkErr->data[ 8*bsize+it] - newErr->data[ 8*bsize+it];
     newErr->data[ 9*bsize+it] = trkErr->data[ 9*bsize+it] - newErr->data[ 9*bsize+it];
     newErr->data[10*bsize+it] = trkErr->data[10*bsize+it] - newErr->data[10*bsize+it];
     newErr->data[11*bsize+it] = trkErr->data[11*bsize+it] - newErr->data[11*bsize+it];
     newErr->data[12*bsize+it] = trkErr->data[12*bsize+it] - newErr->data[12*bsize+it];
     newErr->data[13*bsize+it] = trkErr->data[13*bsize+it] - newErr->data[13*bsize+it];
     newErr->data[14*bsize+it] = trkErr->data[14*bsize+it] - newErr->data[14*bsize+it];
     newErr->data[15*bsize+it] = trkErr->data[15*bsize+it] - newErr->data[15*bsize+it];
     newErr->data[16*bsize+it] = trkErr->data[16*bsize+it] - newErr->data[16*bsize+it];
     newErr->data[17*bsize+it] = trkErr->data[17*bsize+it] - newErr->data[17*bsize+it];
     newErr->data[18*bsize+it] = trkErr->data[18*bsize+it] - newErr->data[18*bsize+it];
     newErr->data[19*bsize+it] = trkErr->data[19*bsize+it] - newErr->data[19*bsize+it];
     newErr->data[20*bsize+it] = trkErr->data[20*bsize+it] - newErr->data[20*bsize+it];
   });

  /*
  MPlexLH K;           // kalman gain, fixme should be L2
  KalmanHTG(rotT00, rotT01, resErr_loc, tempHH); // intermediate term to get kalman gain (H^T*G)
  KalmanGain(psErr, tempHH, K);

  MPlexHV res_glo;   //position residual in global coordinates
  SubtractFirst3(msPar, psPar, res_glo);
  MPlex2V res_loc;   //position residual in local coordinates
  RotateResidulsOnTangentPlane(rotT00,rotT01,res_glo,res_loc);

  //    Chi2Similarity(res_loc, resErr_loc, outChi2);

  MultResidualsAdd(K, psPar, res_loc, outPar);
  MPlexLL tempLL;
  squashPhiMPlex(outPar,N_proc); // ensure phi is between |pi|
  KHMult(K, rotT00, rotT01, tempLL);
  KHC(tempLL, psErr, outErr);
  outErr.Subtract(psErr, outErr);
  */
  
  trkErr = newErr;
}

KOKKOS_FUNCTION inline void sincos4(const float x, float& sin, float& cos)
{
   // Had this writen with explicit division by factorial.
   // The *whole* fitting test ran like 2.5% slower on MIC, sigh.

   const float x2 = x*x;
   cos  = 1.f - 0.5f*x2 + 0.04166667f*x2*x2;
   sin  = x - 0.16666667f*x*x2;
}

constexpr float kfact= 100/3.8;
constexpr int Niter=5;
template <typename member_type>
KOKKOS_FUNCTION void propagateToR(const MP6x6SF* inErr, const MP6F* inPar, const MP1I* inChg, 
                  const MP3F* msP, MP6x6SF* outErr, MP6F* outPar, MP6x6F* errorProp, MP6x6F* temp, const member_type& teamMember) {
  
  //MP6x6F errorProp, temp;
  Kokkos::parallel_for( Kokkos::TeamVectorRange(teamMember,bsize),[&] (const size_t it){ 
    //initialize erroProp to identity matrix
    for (size_t i=0;i<6;++i) errorProp->data[bsize*PosInMtrx(i,i,6) + it] = 1.f;
    
    float r0 = hipo(x(inPar,it), y(inPar,it));
    const float k = q(inChg,it) * kfact;
    const float r = hipo(x(msP,it), y(msP,it));

    const float xin     = x(inPar,it);
    const float yin     = y(inPar,it);
    const float iptin   = ipt(inPar,it);
    const float phiin   = phi(inPar,it);
    const float thetain = theta(inPar,it);

    //initialize outPar to inPar
    setx(outPar,it, xin);
    sety(outPar,it, yin);
    setz(outPar,it, z(inPar,it));
    setipt(outPar,it, iptin);
    setphi(outPar,it, phiin);
    settheta(outPar,it, thetain);

    const float kinv  = 1.f/k;
    const float pt = 1.f/iptin;

    float D = 0., cosa = 0., sina = 0., id = 0.;
    //no trig approx here, phi can be large
    float cosPorT = std::cos(phiin), sinPorT = std::sin(phiin);
    float pxin = cosPorT*pt;
    float pyin = sinPorT*pt;

    //derivatives initialized to value for first iteration, i.e. distance = r-r0in
    float dDdx = r0 > 0.f ? -xin/r0 : 0.f;
    float dDdy = r0 > 0.f ? -yin/r0 : 0.f;
    float dDdipt = 0.;
    float dDdphi = 0.;

    for (int i = 0; i < Niter; ++i)
    {
      //compute distance and path for the current iteration
      r0 = hipo(x(outPar,it), y(outPar,it));
      id = (r-r0);
      D+=id;
      sincos4(id*iptin*kinv, sina, cosa);

      //update derivatives on total distance
      if (i+1 != Niter) {

	const float xtmp = x(outPar,it);
	const float ytmp = y(outPar,it);
	const float oor0 = (r0>0.f && std::abs(r-r0)<0.0001f) ? 1.f/r0 : 0.f;

	const float dadipt = id*kinv;

	const float dadx = -xtmp*iptin*kinv*oor0;
	const float dady = -ytmp*iptin*kinv*oor0;

	const float pxca = pxin*cosa;
	const float pxsa = pxin*sina;
	const float pyca = pyin*cosa;
	const float pysa = pyin*sina;

	float tmp = k*dadx;
	dDdx   -= ( xtmp*(1.f + tmp*(pxca - pysa)) + ytmp*tmp*(pyca + pxsa) )*oor0;
	tmp = k*dady;
	dDdy   -= ( xtmp*tmp*(pxca - pysa) + ytmp*(1.f + tmp*(pyca + pxsa)) )*oor0;
	//now r0 depends on ipt and phi as well
	tmp = dadipt*iptin;
	dDdipt -= k*( xtmp*(pxca*tmp - pysa*tmp - pyca - pxsa + pyin) +
		      ytmp*(pyca*tmp + pxsa*tmp - pysa + pxca - pxin))*pt*oor0;
	dDdphi += k*( xtmp*(pysa - pxin + pxca) - ytmp*(pxsa - pyin + pyca))*oor0;
      }

      //update parameters
      setx(outPar,it, x(outPar,it) + k*(pxin*sina - pyin*(1.f-cosa)));
      sety(outPar,it, y(outPar,it) + k*(pyin*sina + pxin*(1.f-cosa)));
      const float pxinold = pxin;//copy before overwriting
      pxin = pxin*cosa - pyin*sina;
      pyin = pyin*cosa + pxinold*sina;
    }

    const float alpha  = D*iptin*kinv;
    const float dadx   = dDdx*iptin*kinv;
    const float dady   = dDdy*iptin*kinv;
    const float dadipt = (iptin*dDdipt + D)*kinv;
    const float dadphi = dDdphi*iptin*kinv;

    sincos4(alpha, sina, cosa);

    errorProp->data[bsize*PosInMtrx(0,0,6) + it] = 1.f+k*dadx*(cosPorT*cosa-sinPorT*sina)*pt;
    errorProp->data[bsize*PosInMtrx(0,1,6) + it] =     k*dady*(cosPorT*cosa-sinPorT*sina)*pt;
    errorProp->data[bsize*PosInMtrx(0,2,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(0,3,6) + it] = k*(cosPorT*(iptin*dadipt*cosa-sina)+sinPorT*((1.f-cosa)-iptin*dadipt*sina))*pt*pt;
    errorProp->data[bsize*PosInMtrx(0,4,6) + it] = k*(cosPorT*dadphi*cosa - sinPorT*dadphi*sina - sinPorT*sina + cosPorT*cosa - cosPorT)*pt;
    errorProp->data[bsize*PosInMtrx(0,5,6) + it] = 0.f;

    errorProp->data[bsize*PosInMtrx(1,0,6) + it] =     k*dadx*(sinPorT*cosa+cosPorT*sina)*pt;
    errorProp->data[bsize*PosInMtrx(1,1,6) + it] = 1.f+k*dady*(sinPorT*cosa+cosPorT*sina)*pt;
    errorProp->data[bsize*PosInMtrx(1,2,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(1,3,6) + it] = k*(sinPorT*(iptin*dadipt*cosa-sina)+cosPorT*(iptin*dadipt*sina-(1.f-cosa)))*pt*pt;
    errorProp->data[bsize*PosInMtrx(1,4,6) + it] = k*(sinPorT*dadphi*cosa + cosPorT*dadphi*sina + sinPorT*cosa + cosPorT*sina - sinPorT)*pt;
    errorProp->data[bsize*PosInMtrx(1,5,6) + it] = 0.f;

    //no trig approx here, theta can be large
    cosPorT=std::cos(thetain);
    sinPorT=std::sin(thetain);
    //redefine sinPorT as 1./sinPorT to reduce the number of temporaries
    sinPorT = 1.f/sinPorT;

    setz(outPar,it, z(inPar,it) + k*alpha*cosPorT*pt*sinPorT);

    errorProp->data[bsize*PosInMtrx(2,0,6) + it] = k*cosPorT*dadx*pt*sinPorT;
    errorProp->data[bsize*PosInMtrx(2,1,6) + it] = k*cosPorT*dady*pt*sinPorT;
    errorProp->data[bsize*PosInMtrx(2,2,6) + it] = 1.f;
    errorProp->data[bsize*PosInMtrx(2,3,6) + it] = k*cosPorT*(iptin*dadipt-alpha)*pt*pt*sinPorT;
    errorProp->data[bsize*PosInMtrx(2,4,6) + it] = k*dadphi*cosPorT*pt*sinPorT;
    errorProp->data[bsize*PosInMtrx(2,5,6) + it] =-k*alpha*pt*sinPorT*sinPorT;

    setipt(outPar,it, iptin);

    errorProp->data[bsize*PosInMtrx(3,0,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(3,1,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(3,2,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(3,3,6) + it] = 1.f;
    errorProp->data[bsize*PosInMtrx(3,4,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(3,5,6) + it] = 0.f;

    setphi(outPar,it, phi(inPar,it)+alpha );

    errorProp->data[bsize*PosInMtrx(4,0,6) + it] = dadx;
    errorProp->data[bsize*PosInMtrx(4,1,6) + it] = dady;
    errorProp->data[bsize*PosInMtrx(4,2,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(4,3,6) + it] = dadipt;
    errorProp->data[bsize*PosInMtrx(4,4,6) + it] = 1.f+dadphi;
    errorProp->data[bsize*PosInMtrx(4,5,6) + it] = 0.f;

    settheta(outPar,it, thetain);

    errorProp->data[bsize*PosInMtrx(5,0,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(5,1,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(5,2,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(5,3,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(5,4,6) + it] = 0.f;
    errorProp->data[bsize*PosInMtrx(5,5,6) + it] = 1.f;
  });

  MultHelixProp(errorProp, inErr, temp, teamMember);
  MultHelixPropTransp(errorProp, temp, outErr, teamMember);
}

int main (int argc, char* argv[]) {

   int itr;
   ATRK inputtrk ={
     {-12.806846618652344, -7.723824977874756, 38.13014221191406,0.23732035065189902, -2.613372802734375, 0.35594117641448975},
     {6.290299552347278e-07,4.1375109560704004e-08,7.526661534029699e-07,2.0973730840978533e-07,1.5431574240665213e-07,9.626245400795597e-08,-2.804026640189443e-06,
      6.219111130687595e-06,2.649119409845118e-07,0.00253512163402557,-2.419662877381737e-07,4.3124190760040646e-07,3.1068903991780678e-09,0.000923913115050627,
      0.00040678296006807003,-7.755406890332818e-07,1.68539375883925e-06,6.676875566525437e-08,0.0008420574605423793,7.356584799406111e-05,0.0002306247719158348},
     1
   };

   AHIT inputhit = {
     {-20.7824649810791, -12.24150276184082, 57.8067626953125},
     {2.545517190810642e-06,-2.6680759219743777e-06,2.8030024168401724e-06,0.00014160551654640585,0.00012282167153898627,11.385087966918945}
   };

   printf("track in pos: x=%f, y=%f, z=%f, r=%f \n", inputtrk.par[0], inputtrk.par[1], inputtrk.par[2], sqrtf(inputtrk.par[0]*inputtrk.par[0] + inputtrk.par[1]*inputtrk.par[1]));
   printf("track in cov: xx=%.2e, yy=%.2e, zz=%.2e \n", inputtrk.cov[SymOffsets66(PosInMtrx(0,0,6))],
	                                       inputtrk.cov[SymOffsets66(PosInMtrx(1,1,6))],
	                                       inputtrk.cov[SymOffsets66(PosInMtrx(2,2,6))]);
   printf("hit in pos: x=%f, y=%f, z=%f, r=%f \n", inputhit.pos[0], inputhit.pos[1], inputhit.pos[2], sqrtf(inputhit.pos[0]*inputhit.pos[0] + inputhit.pos[1]*inputhit.pos[1]));
   
   printf("produce nevts=%i ntrks=%i smearing by=%f \n", nevts, ntrks, smear);
   printf("NITER=%d\n", NITER);
   printf("Before time setup\n");
   long setup_start, setup_stop;
   struct timeval timecheck;

   gettimeofday(&timecheck, NULL);
   setup_start = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;
   printf("Before kokkos::init\n");

   Kokkos::initialize(argc, argv);
   {

   #ifdef KOKKOS_ENABLE_CUDA
   #define MemSpace Kokkos::CudaSpace
   #endif
   #ifdef KOKKOS_ENABLE_HIP
   #define MemSpace Kokkos::Experimental::HIPSpace
   #endif
   #ifdef KOKKOS_ENABLE_OPENMPTARGET
   #define MemSpace Kokkos::OpenMPTargetSpace
   #endif

   #ifndef MemSpace
   #define MemSpace Kokkos::HostSpace
   #endif

   printf("After kokkos::init\n");
   using ExecSpace = MemSpace::execution_space;
   ExecSpace e;
   e.print_configuration(std::cout, true);

   Kokkos::View<MPTRK*>             trk("trk",nevts*nb); // device pointer
   Kokkos::View<MPTRK*>::HostMirror h_trk = prepareTracks(inputtrk,trk);  // host pointer
   Kokkos::deep_copy( trk , h_trk);

   Kokkos::View<MPHIT*>             hit("hit",nevts*nb*nlayer);
   Kokkos::View<MPHIT*>::HostMirror  h_hit = prepareHits(inputhit,hit);
   Kokkos::deep_copy( hit, h_hit );

   //MPTRK* outtrk = (MPTRK*) malloc(nevts*nb*sizeof(MPTRK));
   Kokkos::View<MPTRK*>             outtrk("outtrk",nevts*nb); // device pointer
   Kokkos::View<MPTRK*>::HostMirror h_outtrk = Kokkos::create_mirror_view( outtrk);
   gettimeofday(&timecheck, NULL);
   setup_stop = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000;

   printf("done preparing!\n");
   
   //using ExecSpace = MemSpace::execution_space;

   //std::vector<int> weights(num_streams);
   //for(int s=0;s<num_streams;s++) weights[s]=1; // equal weights for each steam
   //auto instances = Kokkos::Experimental::partition_space(ExecSpcae,weights)

   //std::vector< Kokkos::TeamPolicy<ExecSpace> >              team_policies(num_streams);
   typedef Kokkos::TeamPolicy<>               team_policy;
   typedef Kokkos::TeamPolicy<>::member_type  member_type;

   using mp6x6F_view_type  = Kokkos::View< MP6x6F,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;
   using mp1F_view_type    = Kokkos::View< MP1F,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;
   using mp2x2SF_view_type = Kokkos::View< MP2x2SF,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;
   using mp3x6_view_type   = Kokkos::View< MP3x6,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;
   using mp2F_view_type    = Kokkos::View< MP2F,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;
   using mp6x6SF_view_type = Kokkos::View< MP6x6SF,Kokkos::DefaultExecutionSpace::scratch_memory_space,Kokkos::MemoryTraits<Kokkos::Unmanaged> >;

   size_t mp6x6F_bytes       = 2*mp6x6F_view_type::shmem_size();
   size_t mp1F_bytes         = 2*mp1F_view_type::shmem_size();
   size_t mp2x2SF_bytes      = mp2x2SF_view_type::shmem_size();
   size_t mp3x6_view_bytes   = mp3x6_view_type::shmem_size();
   size_t mp2F_view_bytes    = mp2F_view_type::shmem_size();
   size_t mp6x6SF_view_bytes = mp6x6SF_view_type::shmem_size();

   auto total_shared_bytes =mp6x6F_bytes+mp1F_bytes+mp2x2SF_bytes+mp3x6_view_bytes+mp2F_view_bytes+mp6x6SF_view_bytes ;

   int shared_view_level = 0;

   //int team_policy_range = nevts;
   int team_policy_range = nevts*nb;  // number of teams
   int team_size = threadsperblockx;  // team size
   int vector_size = elementsperthread;  // thread size
   auto wall_start = std::chrono::high_resolution_clock::now();

   for(itr=0; itr<NITER; itr++) {
      Kokkos::parallel_for("Kernel", team_policy(team_policy_range,team_size,vector_size).set_scratch_size( 0, Kokkos::PerTeam( total_shared_bytes )), 
                                    KOKKOS_LAMBDA( const member_type &teamMember){
        int ie = teamMember.league_rank()/nb;
        int ib = teamMember.league_rank()% nb;

        mp6x6F_view_type errorProp( teamMember.team_scratch(shared_view_level) );
        mp6x6F_view_type temp ( teamMember.team_scratch(shared_view_level));
        mp1F_view_type   rotT00( teamMember.team_scratch(shared_view_level));
        mp1F_view_type   rotT01( teamMember.team_scratch(shared_view_level));
        mp2x2SF_view_type resErr_loc( teamMember.team_scratch(shared_view_level));
        mp3x6_view_type   kGain ( teamMember.team_scratch(shared_view_level));
        mp2F_view_type    res_loc( teamMember.team_scratch(shared_view_level));
        mp6x6SF_view_type  newErr( teamMember.team_scratch(shared_view_level));

          MPTRK* btracks = bTk(trk, ie, ib);
          MPTRK* obtracks = bTk(outtrk, ie, ib);
          for(size_t layer=0; layer<nlayer; ++layer) {
            const MPHIT* bhits = bHit(hit, ie, ib, layer);
            propagateToR(&(*btracks).cov, &(*btracks).par, &(*btracks).q, &(*bhits).pos,
                          &(*obtracks).cov, &(*obtracks).par,(errorProp.data()),(temp.data()),teamMember);
            KalmanUpdate(&(*obtracks).cov,&(*obtracks).par,&(*bhits).cov,&(*bhits).pos,
                        (rotT00.data()), (rotT01.data()), (resErr_loc.data()), (kGain.data()), (res_loc.data()), (newErr.data()),teamMember);
          }
        //});
     });
   } //end of itr loop

   //Syncthreads
   Kokkos::fence();
   auto wall_stop = std::chrono::high_resolution_clock::now();

   auto wall_diff = wall_stop - wall_start;
   auto wall_time = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(wall_diff).count()) / 1e6;
   printf("setup time time=%f (s)\n", (setup_stop-setup_start)*0.001);
   printf("done ntracks=%i tot time=%f (s) time/trk=%e (s)\n", nevts*ntrks*int(NITER), wall_time, wall_time/(nevts*ntrks*int(NITER)));
   printf("formatted %i %i %i %i %i %f 0 %f\n",int(NITER),nevts, ntrks, bsize, nb, wall_time, (setup_stop-setup_start)*0.001 );

   //Copy back to host 
   Kokkos::deep_copy( h_outtrk, outtrk );

   float avgx = 0, avgy = 0, avgz = 0, avgr = 0;
   float avgpt = 0, avgphi = 0, avgtheta = 0;
   float avgdx = 0, avgdy = 0, avgdz = 0, avgdr = 0;
   for (size_t ie=0;ie<nevts;++ie) {
     for (size_t it=0;it<ntrks;++it) {
       float x_ = x(h_outtrk.data(),ie,it);
       float y_ = y(h_outtrk.data(),ie,it);
       float z_ = z(h_outtrk.data(),ie,it);
       float r_ = sqrtf(x_*x_ + y_*y_);
       float pt_ = 1./ipt(h_outtrk.data(),ie,it);
       float phi_ = phi(h_outtrk.data(),ie,it);
       float theta_ = theta(h_outtrk.data(),ie,it);
       avgpt += pt_;
       avgphi += phi_;
       avgtheta += theta_;
       avgx += x_;
       avgy += y_;
       avgz += z_;
       avgr += r_;
       float hx_ = x(h_hit.data(),ie,it);
       float hy_ = y(h_hit.data(),ie,it);
       float hz_ = z(h_hit.data(),ie,it);
       float hr_ = sqrtf(hx_*hx_ + hy_*hy_);
       //printf("ie=%i, it=%i, hx_ =%f , x_=%f \n", ie, it, hx_,x_);
       avgdx += (x_-hx_)/x_;
       avgdy += (y_-hy_)/y_;
       avgdz += (z_-hz_)/z_;
       avgdr += (r_-hr_)/r_;
     }
   }
   avgpt = avgpt/float(nevts*ntrks);
   avgphi = avgphi/float(nevts*ntrks);
   avgtheta = avgtheta/float(nevts*ntrks);
   avgx = avgx/float(nevts*ntrks);
   avgy = avgy/float(nevts*ntrks);
   avgz = avgz/float(nevts*ntrks);
   avgr = avgr/float(nevts*ntrks);
   avgdx = avgdx/float(nevts*ntrks);
   avgdy = avgdy/float(nevts*ntrks);
   avgdz = avgdz/float(nevts*ntrks);
   avgdr = avgdr/float(nevts*ntrks);

   float stdx = 0, stdy = 0, stdz = 0, stdr = 0;
   float stddx = 0, stddy = 0, stddz = 0, stddr = 0;
   for (size_t ie=0;ie<nevts;++ie) {
     for (size_t it=0;it<ntrks;++it) {
       float x_ = x(h_outtrk.data(),ie,it);
       float y_ = y(h_outtrk.data(),ie,it);
       float z_ = z(h_outtrk.data(),ie,it);
       float r_ = sqrtf(x_*x_ + y_*y_);
       stdx += (x_-avgx)*(x_-avgx);
       stdy += (y_-avgy)*(y_-avgy);
       stdz += (z_-avgz)*(z_-avgz);
       stdr += (r_-avgr)*(r_-avgr);
       float hx_ = x(h_hit.data(),ie,it);
       float hy_ = y(h_hit.data(),ie,it);
       float hz_ = z(h_hit.data(),ie,it);
       float hr_ = sqrtf(hx_*hx_ + hy_*hy_);
       stddx += ((x_-hx_)/x_-avgdx)*((x_-hx_)/x_-avgdx);
       stddy += ((y_-hy_)/y_-avgdy)*((y_-hy_)/y_-avgdy);
       stddz += ((z_-hz_)/z_-avgdz)*((z_-hz_)/z_-avgdz);
       stddr += ((r_-hr_)/r_-avgdr)*((r_-hr_)/r_-avgdr);
     }
   }

   stdx = sqrtf(stdx/float(nevts*ntrks));
   stdy = sqrtf(stdy/float(nevts*ntrks));
   stdz = sqrtf(stdz/float(nevts*ntrks));
   stdr = sqrtf(stdr/float(nevts*ntrks));
   stddx = sqrtf(stddx/float(nevts*ntrks));
   stddy = sqrtf(stddy/float(nevts*ntrks));
   stddz = sqrtf(stddz/float(nevts*ntrks));
   stddr = sqrtf(stddr/float(nevts*ntrks));

   printf("track x avg=%f std/avg=%f\n", avgx, fabs(stdx/avgx));
   printf("track y avg=%f std/avg=%f\n", avgy, fabs(stdy/avgy));
   printf("track z avg=%f std/avg=%f\n", avgz, fabs(stdz/avgz));
   printf("track r avg=%f std/avg=%f\n", avgr, fabs(stdr/avgz));
   printf("track dx/x avg=%f std=%f\n", avgdx, stddx);
   printf("track dy/y avg=%f std=%f\n", avgdy, stddy);
   printf("track dz/z avg=%f std=%f\n", avgdz, stddz);
   printf("track dr/r avg=%f std=%f\n", avgdr, stddr);
   printf("track pt avg=%f\n", avgpt);
   printf("track phi avg=%f\n", avgphi);
   printf("track theta avg=%f\n", avgtheta);

   //free(trk);
   //free(hit);
   //free(outtrk);
   };
   Kokkos::finalize();

   return 0;
}
