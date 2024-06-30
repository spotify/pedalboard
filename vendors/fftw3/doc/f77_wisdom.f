c     Copyright (c) 2003, 2007-14 Matteo Frigo
c     Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
c     
c     This program is free software; you can redistribute it and/or modify
c     it under the terms of the GNU General Public License as published by
c     the Free Software Foundation; either version 2 of the License, or
c     (at your option) any later version.
c     
c     This program is distributed in the hope that it will be useful,
c     but WITHOUT ANY WARRANTY; without even the implied warranty of
c     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
c     GNU General Public License for more details.
c     
c     You should have received a copy of the GNU General Public License
c     along with this program; if not, write to the Free Software
c     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
c
cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc
c     
c     This is an example implementation of Fortran wisdom export/import
c     to/from a Fortran unit (file), exploiting the generic
c     dfftw_export_wisdom/dfftw_import_wisdom functions.
c     
c     We cannot compile this file into the FFTW library itself, lest all
c     FFTW-calling programs be required to link to the Fortran I/O
c     libraries.
c     
cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc

c     Strictly speaking, the '$' format specifier, which allows us to
c     write a character without a trailing newline, is not standard F77.
c     However, it seems to be a nearly universal extension.
      subroutine write_char(c, iunit)
      character c
      integer iunit
      write(iunit,321) c
 321  format(a,$)
      end      

      subroutine export_wisdom_to_file(iunit)
      integer iunit
      external write_char
      call dfftw_export_wisdom(write_char, iunit)
      end

c     Fortran 77 does not have any portable way to read an arbitrary
c     file one character at a time.  The best alternative seems to be to
c     read a whole line into a buffer, since for fftw-exported wisdom we
c     can bound the line length.  (If the file contains longer lines,
c     then the lines will be truncated and the wisdom import should
c     simply fail.)  Ugh.
      subroutine read_char(ic, iunit)
      integer ic
      integer iunit
      character*256 buf
      save buf
      integer ibuf
      data ibuf/257/
      save ibuf
      if (ibuf .lt. 257) then
         ic = ichar(buf(ibuf:ibuf))
         ibuf = ibuf + 1
         return
      endif
      read(iunit,123,end=666) buf
      ic = ichar(buf(1:1))
      ibuf = 2
      return
 666  ic = -1
      ibuf = 257
 123  format(a256)
      end
      
      subroutine import_wisdom_from_file(isuccess, iunit)
      integer isuccess
      integer iunit
      external read_char
      call dfftw_import_wisdom(isuccess, read_char, iunit)
      end
