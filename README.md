# GMime

[![Build Status](https://travis-ci.org/jstedfast/gmime.svg)](https://travis-ci.org/jstedfast/gmime)[![Coverity Scan Build Status](https://scan.coverity.com/projects/11948/badge.svg)](https://scan.coverity.com/projects/jstedfast-gmime)[![Coverage Status](https://coveralls.io/repos/github/jstedfast/gmime/badge.svg?branch=master)](https://coveralls.io/github/jstedfast/gmime?branch=master)

## What is GMime?

GMime is a C/C++ library which may be used for the creation and parsing of messages using the Multipurpose
Internet Mail Extension (MIME) as defined by [numerous IETF specifications](https://github.com/jstedfast/gmime/blob/master/RFCs.md).

## History

As a developer and user of Electronic Mail clients, I had come to
realize that the vast majority of E-Mail client (and server) software
had less-than-satisfactory MIME implementations. More often than not
these E-Mail clients created broken MIME messages and/or would
incorrectly try to parse a MIME message thus subtracting from the full
benefits that MIME was meant to provide. GMime is meant to address
this issue by following the MIME specification as closely as possible
while also providing programmers with an extremely easy to use
high-level application programming interface (API).

## License Information

The GMime library is Copyright (C) 2000-2017 Jeffrey Stedfast and is licensed under the LGPL v2.1

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


## Getting the Source Code

You can download either official public release tarballs of GMime at
[http://download.gnome.org/pub/GNOME/sources/gmime/](http://download.gnome.org/pub/GNOME/sources/gmime/)
or
[ftp://ftp.gnome.org/pub/GNOME/sources/gmime/](ftp://ftp.gnome.org/pub/GNOME/sources/gmime/).

If you would like to contribute to the GMime project, it is recommended that you grab the
source code from the official GitHub repository at
[http://github.com/jstedfast/gmime](https://github.com/jstedfast/gmime). Cloning this repository
can be done using the following command:

    git clone https://github.com/jstedfast/gmime.git


## Requirements

For proper compilation and functionality of GMime, the following packages
are REQUIRED:

  - Glib version >= 2.32.0

    Glib provides a number of portability-enhancing functions and types.
    Glib is included in most GMime-supported operating system
    distributions.  Glib sources may be obtained from:
      ftp://ftp.gtk.org/pub/glib


## Documentation

This is the README file for GMime. Additional documentation related to
development using GMime has been included within the source release
of GMime.

  docs/reference/       Contains SGML and HTML versions of the GMime
                        reference manual

  docs/tutorial/        Contains SGML and HTML versions of the GMime
                        tutorial

  AUTHORS               List of primary authors (source code developers)

  COPYING               The GNU Lesser General Public License, version 2

  ChangeLog             Log of changes made to the source code

  INSTALL               In-depth installation instructions

  NEWS                  Release notes (Overview of changes)

  TODO                  Description of planned GMime development

  PORTING               Guide for developers porting their application
                        from an older version of GMime


You can find online developer documentation at
http://library.gnome.org/devel/gmime/stable/


# Mailing Lists

For discussion of GMime development (either of GMime itself or using
GMime in your own application), you may find the GMime-Devel
mailing-list helpful. To subscribe, please see
[http://mail.gnome.org/mailman/listinfo/gmime-devel-list](http://mail.gnome.org/mailman/listinfo/gmime-devel-list)


# Bindings

Other developers have been working to make GMime available to programmers in other languages.
The current list of known bindings are:

- Go: https://github.com/sendgrid/go-gmime

- Perl: http://search.cpan.org/dist/MIME-Fast/

- .NET (Mono): Included in this distribution (Obsolete).


Note: It is recommended that [MimeKit](https://github.com/jstedfast/MimeKit) be used in place of GMime-Sharp in
.NET 4.0 (or later) environments.


# Reporting Bugs

Bugs may be reported to the GMime development team by creating a new
[issue](https://github.com/jstedfast/gmime/issues).
