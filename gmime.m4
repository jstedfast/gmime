# Configure paths for GMime
# Charlie Schmidt 00-12-2
# nearly word for word from gtk.m4 by Owen Taylor


AC_DEFUN(AM_PATH_GMIME,
[dnl 
dnl Get the cflags and libraries from the gmime-config script
dnl
AC_ARG_WITH(gmime-prefix,[  --with-gmime-prefix=PFX   Prefix where GMIME is installed (optional)],
            gmime_config_prefix="$withval", gmime_config_prefix="")
AC_ARG_WITH(gmime-exec-prefix,[  --with-gmime-exec-prefix=PFX Exec prefix where GMIME is installed (optional)],
            gmime_config_exec_prefix="$withval", gmime_config_exec_prefix="")
AC_ARG_ENABLE(gmimetest, [  --disable-gmimetest       Do not try to compile and run a test GMIME program],
                    , enable_gmimetest=yes)

  if test x$gmime_config_exec_prefix != x ; then
     gmime_config_args="$gmime_config_args --exec-prefix=$gmime_config_exec_prefix"
     if test x${GMIME_CONFIG+set} != xset ; then
        GMIME_CONFIG=$gmime_config_exec_prefix/bin/gmime-config
     fi
  fi
  if test x$gmime_config_prefix != x ; then
     gmime_config_args="$gmime_config_args --prefix=$gmime_config_prefix"
     if test x${GMIME_CONFIG+set} != xset ; then
        GMIME_CONFIG=$gmime_config_prefix/bin/gmime-config
     fi
  fi

  AC_PATH_PROG(GMIME_CONFIG, gmime-config, no)
  min_gmime_version=ifelse([$1], ,0.2.0,$1)
  AC_MSG_CHECKING(for GMIME - version >= $min_gmime_version)
  no_gmime=""
  if test "$GMIME_CONFIG" = "no" ; then
    no_gmime=yes
  else
    GMIME_CFLAGS=`$GMIME_CONFIG $gmime_config_args --cflags`
    GMIME_LIBS=`$GMIME_CONFIG $gmime_config_args --libs`
    gmime_config_major_version=`$GMIME_CONFIG $gmime_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gmime_config_minor_version=`$GMIME_CONFIG $gmime_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gmime_config_micro_version=`$GMIME_CONFIG $gmime_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gmimetest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GMIME_CFLAGS"
      LIBS="$GMIME_LIBS $LIBS"
dnl
dnl Now check if the installed GMIME is sufficiently new. (Also sanity
dnl checks the results of gmime-config to some extent
dnl
      rm -f conf.gmimetest
      AC_TRY_RUN([
#include <gmime/gmime.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gmimetest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gmime_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gmime_version");
     exit(1);
   }

  if ((gmime_major_version != $gmime_config_major_version) ||
      (gmime_minor_version != $gmime_config_minor_version) ||
      (gmime_micro_version != $gmime_config_micro_version))
    {
      printf("\n*** 'gmime-config --version' returned %d.%d.%d, but GMIME (%d.%d.%d)\n", 
             $gmime_config_major_version, $gmime_config_minor_version, $gmime_config_micro_version,
             gmime_major_version, gmime_minor_version, gmime_micro_version);
      printf ("*** was found! If gmime-config was correct, then it is best\n");
      printf ("*** to remove the old version of GMIME. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gmime-config was wrong, set the environment variable GMIME_CONFIG\n");
      printf("*** to point to the correct copy of gmime-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (GMIME_MAJOR_VERSION) && defined (GMIME_MINOR_VERSION) && defined (GMIME_MICRO_VERSION)
  else if ((gmime_major_version != GMIME_MAJOR_VERSION) ||
           (gmime_minor_version != GMIME_MINOR_VERSION) ||
           (gmime_micro_version != GMIME_MICRO_VERSION))
    {
      printf("*** GMIME header files (version %d.%d.%d) do not match\n",
             GMIME_MAJOR_VERSION, GMIME_MINOR_VERSION, GMIME_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
             gmime_major_version, gmime_minor_version, gmime_micro_version);
    }
#endif /* defined (GMIME_MAJOR_VERSION) ... */
  else
    {
      if ((gmime_major_version > major) ||
        ((gmime_major_version == major) && (gmime_minor_version > minor)) ||
        ((gmime_major_version == major) && (gmime_minor_version == minor) && (gmime_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GMIME (%d.%d.%d) was found.\n",
               gmime_major_version, gmime_minor_version, gmime_micro_version);
        printf("*** You need a version of GMIME newer than %d.%d.%d. The latest version of\n",
               major, minor, micro);
        printf("*** GMIME is always available from http://www.xtorshun.org/gmime/.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gmime-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GMIME+, but you can also set the GMIME_CONFIG environment to point to the\n");
        printf("*** correct copy of gmime-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gmime=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gmime" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GMIME_CONFIG" = "no" ; then
       echo "*** The gmime-config script installed by GMIME could not be found"
       echo "*** If GMIME was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GMIME_CONFIG environment variable to the"
       echo "*** full path to gmime-config."
     else
       if test -f conf.gmimetest ; then
        :
       else
          echo "*** Could not run GMIME test program, checking why..."
          CFLAGS="$CFLAGS $GMIME_CFLAGS"
          LIBS="$LIBS $GMIME_LIBS"
          AC_TRY_LINK([
#include <gmime/gmime.h>
#include <stdio.h>
],      [ return ((gmime_major_version) || (gmime_minor_version) || (gmime_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GMIME or finding the wrong"
          echo "*** version of GMIME. If it is not finding GMIME, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
          echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GMIME was incorrectly installed"
          echo "*** or that you have moved GMIME since it was installed. In the latter case, you"
          echo "*** may want to edit the gmime-config script: $GMIME_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GMIME_CFLAGS=""
     GMIME_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GMIME_CFLAGS)
  AC_SUBST(GMIME_LIBS)
  rm -f conf.gmimetest
])
