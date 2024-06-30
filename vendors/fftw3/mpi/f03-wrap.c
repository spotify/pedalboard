/* Generated automatically.  DO NOT EDIT! */

#include "fftw3-mpi.h"
#include "ifftw-mpi.h"

FFTW_EXTERN ptrdiff_t XM(local_size_many_transposed_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block0, ptrdiff_t block1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start);
FFTW_EXTERN ptrdiff_t XM(local_size_many_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block0, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start);
FFTW_EXTERN ptrdiff_t XM(local_size_transposed_f03)(int rnk, const ptrdiff_t * n, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start);
FFTW_EXTERN ptrdiff_t XM(local_size_f03)(int rnk, const ptrdiff_t * n, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start);
FFTW_EXTERN ptrdiff_t XM(local_size_many_1d_f03)(ptrdiff_t n0, ptrdiff_t howmany, MPI_Fint f_comm, int sign, unsigned flags, ptrdiff_t * local_ni, ptrdiff_t * local_i_start, ptrdiff_t * local_no, ptrdiff_t * local_o_start);
FFTW_EXTERN ptrdiff_t XM(local_size_1d_f03)(ptrdiff_t n0, MPI_Fint f_comm, int sign, unsigned flags, ptrdiff_t * local_ni, ptrdiff_t * local_i_start, ptrdiff_t * local_no, ptrdiff_t * local_o_start);
FFTW_EXTERN ptrdiff_t XM(local_size_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start);
FFTW_EXTERN ptrdiff_t XM(local_size_2d_transposed_f03)(ptrdiff_t n0, ptrdiff_t n1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start);
FFTW_EXTERN ptrdiff_t XM(local_size_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start);
FFTW_EXTERN ptrdiff_t XM(local_size_3d_transposed_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start);
FFTW_EXTERN X(plan) XM(plan_many_transpose_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t howmany, ptrdiff_t block0, ptrdiff_t block1, R * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_transpose_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_many_dft_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block, ptrdiff_t tblock, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_f03)(int rnk, const ptrdiff_t * n, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_1d_f03)(ptrdiff_t n0, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_many_r2r_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, R * in, R * out, MPI_Fint f_comm, const X(r2r_kind) * kind, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_r2r_f03)(int rnk, const ptrdiff_t * n, R * in, R * out, MPI_Fint f_comm, const X(r2r_kind) * kind, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_r2r_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, R * out, MPI_Fint f_comm, X(r2r_kind) kind0, X(r2r_kind) kind1, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_r2r_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, R * in, R * out, MPI_Fint f_comm, X(r2r_kind) kind0, X(r2r_kind) kind1, X(r2r_kind) kind2, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_many_dft_r2c_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_r2c_f03)(int rnk, const ptrdiff_t * n, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_r2c_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_r2c_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_many_dft_c2r_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_c2r_f03)(int rnk, const ptrdiff_t * n, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_c2r_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN X(plan) XM(plan_dft_c2r_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags);
FFTW_EXTERN void XM(gather_wisdom_f03)(MPI_Fint f_comm_);
FFTW_EXTERN void XM(broadcast_wisdom_f03)(MPI_Fint f_comm_);

ptrdiff_t XM(local_size_many_transposed_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block0, ptrdiff_t block1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_many_transposed)(rnk,n,howmany,block0,block1,comm,local_n0,local_0_start,local_n1,local_1_start);
}

ptrdiff_t XM(local_size_many_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block0, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_many)(rnk,n,howmany,block0,comm,local_n0,local_0_start);
}

ptrdiff_t XM(local_size_transposed_f03)(int rnk, const ptrdiff_t * n, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_transposed)(rnk,n,comm,local_n0,local_0_start,local_n1,local_1_start);
}

ptrdiff_t XM(local_size_f03)(int rnk, const ptrdiff_t * n, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size)(rnk,n,comm,local_n0,local_0_start);
}

ptrdiff_t XM(local_size_many_1d_f03)(ptrdiff_t n0, ptrdiff_t howmany, MPI_Fint f_comm, int sign, unsigned flags, ptrdiff_t * local_ni, ptrdiff_t * local_i_start, ptrdiff_t * local_no, ptrdiff_t * local_o_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_many_1d)(n0,howmany,comm,sign,flags,local_ni,local_i_start,local_no,local_o_start);
}

ptrdiff_t XM(local_size_1d_f03)(ptrdiff_t n0, MPI_Fint f_comm, int sign, unsigned flags, ptrdiff_t * local_ni, ptrdiff_t * local_i_start, ptrdiff_t * local_no, ptrdiff_t * local_o_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_1d)(n0,comm,sign,flags,local_ni,local_i_start,local_no,local_o_start);
}

ptrdiff_t XM(local_size_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_2d)(n0,n1,comm,local_n0,local_0_start);
}

ptrdiff_t XM(local_size_2d_transposed_f03)(ptrdiff_t n0, ptrdiff_t n1, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_2d_transposed)(n0,n1,comm,local_n0,local_0_start,local_n1,local_1_start);
}

ptrdiff_t XM(local_size_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_3d)(n0,n1,n2,comm,local_n0,local_0_start);
}

ptrdiff_t XM(local_size_3d_transposed_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, MPI_Fint f_comm, ptrdiff_t * local_n0, ptrdiff_t * local_0_start, ptrdiff_t * local_n1, ptrdiff_t * local_1_start)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(local_size_3d_transposed)(n0,n1,n2,comm,local_n0,local_0_start,local_n1,local_1_start);
}

X(plan) XM(plan_many_transpose_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t howmany, ptrdiff_t block0, ptrdiff_t block1, R * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_many_transpose)(n0,n1,howmany,block0,block1,in,out,comm,flags);
}

X(plan) XM(plan_transpose_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_transpose)(n0,n1,in,out,comm,flags);
}

X(plan) XM(plan_many_dft_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t block, ptrdiff_t tblock, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_many_dft)(rnk,n,howmany,block,tblock,in,out,comm,sign,flags);
}

X(plan) XM(plan_dft_f03)(int rnk, const ptrdiff_t * n, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft)(rnk,n,in,out,comm,sign,flags);
}

X(plan) XM(plan_dft_1d_f03)(ptrdiff_t n0, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_1d)(n0,in,out,comm,sign,flags);
}

X(plan) XM(plan_dft_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_2d)(n0,n1,in,out,comm,sign,flags);
}

X(plan) XM(plan_dft_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, X(complex) * in, X(complex) * out, MPI_Fint f_comm, int sign, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_3d)(n0,n1,n2,in,out,comm,sign,flags);
}

X(plan) XM(plan_many_r2r_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, R * in, R * out, MPI_Fint f_comm, const X(r2r_kind) * kind, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_many_r2r)(rnk,n,howmany,iblock,oblock,in,out,comm,kind,flags);
}

X(plan) XM(plan_r2r_f03)(int rnk, const ptrdiff_t * n, R * in, R * out, MPI_Fint f_comm, const X(r2r_kind) * kind, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_r2r)(rnk,n,in,out,comm,kind,flags);
}

X(plan) XM(plan_r2r_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, R * out, MPI_Fint f_comm, X(r2r_kind) kind0, X(r2r_kind) kind1, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_r2r_2d)(n0,n1,in,out,comm,kind0,kind1,flags);
}

X(plan) XM(plan_r2r_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, R * in, R * out, MPI_Fint f_comm, X(r2r_kind) kind0, X(r2r_kind) kind1, X(r2r_kind) kind2, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_r2r_3d)(n0,n1,n2,in,out,comm,kind0,kind1,kind2,flags);
}

X(plan) XM(plan_many_dft_r2c_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_many_dft_r2c)(rnk,n,howmany,iblock,oblock,in,out,comm,flags);
}

X(plan) XM(plan_dft_r2c_f03)(int rnk, const ptrdiff_t * n, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_r2c)(rnk,n,in,out,comm,flags);
}

X(plan) XM(plan_dft_r2c_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_r2c_2d)(n0,n1,in,out,comm,flags);
}

X(plan) XM(plan_dft_r2c_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, R * in, X(complex) * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_r2c_3d)(n0,n1,n2,in,out,comm,flags);
}

X(plan) XM(plan_many_dft_c2r_f03)(int rnk, const ptrdiff_t * n, ptrdiff_t howmany, ptrdiff_t iblock, ptrdiff_t oblock, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_many_dft_c2r)(rnk,n,howmany,iblock,oblock,in,out,comm,flags);
}

X(plan) XM(plan_dft_c2r_f03)(int rnk, const ptrdiff_t * n, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_c2r)(rnk,n,in,out,comm,flags);
}

X(plan) XM(plan_dft_c2r_2d_f03)(ptrdiff_t n0, ptrdiff_t n1, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_c2r_2d)(n0,n1,in,out,comm,flags);
}

X(plan) XM(plan_dft_c2r_3d_f03)(ptrdiff_t n0, ptrdiff_t n1, ptrdiff_t n2, X(complex) * in, R * out, MPI_Fint f_comm, unsigned flags)
{
     MPI_Comm comm;

     comm = MPI_Comm_f2c(f_comm);
     return XM(plan_dft_c2r_3d)(n0,n1,n2,in,out,comm,flags);
}

void XM(gather_wisdom_f03)(MPI_Fint f_comm_)
{
     MPI_Comm comm_;

     comm_ = MPI_Comm_f2c(f_comm_);
     XM(gather_wisdom)(comm_);
}

void XM(broadcast_wisdom_f03)(MPI_Fint f_comm_)
{
     MPI_Comm comm_;

     comm_ = MPI_Comm_f2c(f_comm_);
     XM(broadcast_wisdom)(comm_);
}
