/*  secalinux: Emulates a SECA Smartcard

    Copyright (C) 2000  2tor (2tor@zdnetonebox.com)
    Part of original code and algorithms by Fullcrack (fullcrack@teleline.es)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


    This software is for educational purposes only. It should not be used
    to ilegally watch TV channels.

    Version 0.1, December 2000.  
*/

void decrypt_seca( unsigned char *k,unsigned char *d );
void encrypt_seca( unsigned char *k,unsigned char *d );
