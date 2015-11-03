#!/bin/sh
#
# Copyright (c) 2015 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

format="%-32s %s\n"

printf "$format" "NAME" "VALUE"

if [ -d /var/state ] ; then
	find /var/state/ -type f | sort | while read path
	do
		key=`basename $path`
		# FIXME: iseek=8 assumes 64-bit size_t, which will fail on 32-bit machines
		printf "$format" "$key" "`dd if=$path bs=1 iseek=8 status=none`"
	done 
fi

if [ -d $HOME/.libstate/run ] ; then
	find $HOME/.libstate/run -type f | sort | while read path 
	do
		key=`basename $path`
		printf "$format" "user.$key" "`dd if=$path bs=1 iseek=8 status=none`"
	done
fi
