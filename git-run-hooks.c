/*-
 * Generic Git hook program, which runs all hooks from the <basedir>,
 * specified using the -b option on the command line, in alphabetic
 * order. It will pass all additional command line parameters and all
 * input from stdin to each of the hooks.
 *
 * Copyright (c) 2009 Benedikt Meurer <benedikt.meurer@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


/* --- global variables --- */
static const char *progname;


/* --- functions --- */
static void xerr(int code, const char *format, ...)
{
  const char *errmsg;
  va_list ap;

  errmsg = strerror(errno);
  va_start(ap, format);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, " (%s)\n", errmsg);
  fflush(stderr);
  va_end(ap);
  exit(code);
}


static void xerrx(int code, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(ap);
  exit(code);
}


static void *xmalloc(size_t n)
{
  void *p;

  p = malloc(n);
  if (p == NULL)
    xerr(EXIT_FAILURE, "Failed to allocate %d bytes of memory", (int)n);
  return p;
}


static void *xrealloc(void *p, size_t n)
{
  p = (p == NULL) ? malloc(n) : realloc(p, n);
  if (p == NULL)
    xerr(EXIT_FAILURE, "Failed to allocate %d bytes of memory", (int)n);
  return p;
}


static void *xstrdup(const char *s)
{
  return (s == NULL) ? NULL : strcpy((char *)xmalloc(strlen(s) + 1), s);
}


static char *buildfilename(const char *s, ...)
{
  va_list ap;
  char *path;
  size_t n;

  path = xstrdup(s);
  va_start(ap, s);
  for (;;) {
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/')
      path[--n] = '\0';
    s = va_arg(ap, const char *);
    if (s == NULL)
      break;
    path = (char *)xrealloc(path, n + strlen(s) + 2);
    strcat(path, "/");
    strcat(path, s);
  }
  va_end(ap);

  return path;
}


static int xstrpcmp(const void *s1, const void *s2)
{
  return strcmp(*((const char **) s1), *((const char **) s2));
}


static void sighandler(int signo)
{
  (void)signo;
}


static void usage(int code)
{
  fprintf(stderr,
          "Usage: %s -b BASEDIR -- [ARGS]\n"
          "\n"
          "Options:\n"
          " -b BASEDIR  : Specify the BASEDIR of the hooks to execute\n"
          "               (i.e. /path/to/update.d for the update hook)\n"
          "\n"
          "Runs all hooks from the specified BASEDIR, passing them the\n"
          "remaining ARGS and all data from stdin. If BASEDIR does not\n"
          "exist, this program terminates immediately with an exit code\n"
          "of 0.\n",
          progname);
  exit(code);
}


int main(int argc, char **argv) 
{
  const char *basedir = NULL;
  unsigned n, nhooks = 0;
  ssize_t i, j, k;
  char buffer[1024] = "/tmp/git-generic-hook.XXXXXX";
  struct dirent *d;
  DIR *dp;
  char *path, **nargv, **hooks = NULL, *s;
  int ch, fd, status;
  pid_t pid, child;

  /* setup SIGCHLD and SIGPIPE */
  signal(SIGCHLD, sighandler);
  signal(SIGPIPE, sighandler);

  /* figure out the progname (basename of argv[0]) */
  for (path = s = argv[0]; *s != '\0'; ++s)
    if (*s == '/')
      path = s + 1;
  progname = (*path != '\0') ? path : argv[0];

  /* parse the command line parameters */
  while ((ch = getopt(argc, argv, "b:h")) != -1) {
    switch (ch) {
    case 'b':
      basedir = optarg;
      break;

    case 'h':
    case '?':
      usage(EXIT_SUCCESS);

    default:
      usage(EXIT_FAILURE);
    }
  }
  argc -= optind;
  argv += optind;

  /* make sure that -b was specified */
  if (basedir == NULL)
    usage(EXIT_FAILURE);

  /* make sure that $GIT_DIR is set */
  if (getenv("GIT_DIR") == NULL)
    xerrx(EXIT_FAILURE, "GIT_DIR is unset");
  
  /* create a temporary file and immediately unlink it */
  fd = mkstemp(buffer);
  if (fd < 0)
    xerr(EXIT_FAILURE, "Failed to create temporary file");
  if (unlink(buffer) < 0 && errno != ENOENT)
    xerr(EXIT_FAILURE, "Failed to unlink temporary file");
    
  /* write stdin to the temporary file */
  while ((i = read(STDIN_FILENO, buffer, 1024)) != 0) {
    if (i < 0)
      xerr(EXIT_FAILURE, "Failed to read from stdin");
    for (j = 0; j < i; ) {
      k = write(fd, buffer + j, i - j);
      if (k < 0)
        xerr(EXIT_FAILURE, "Failed to write to temporary file");
      j += k;
    }
  }

  /* use temporary file as stdin from now on */
  if (dup2(fd, STDIN_FILENO) < 0)
    xerr(EXIT_FAILURE, "Failed to read from temporary file");
  if (close(fd) < 0)
    xerr(EXIT_FAILURE, "Failed to close temporary file");

  /* try to open the basedir */
  dp = opendir(basedir);
  if (dp == NULL && errno != ENOENT) {
    xerr(EXIT_FAILURE, "Failed to open directory %s", basedir);
  }
  else if (dp != NULL) {
    /* read the hooks from the basedir */
    while ((d = readdir(dp)) != NULL) {
      if (*d->d_name == '.')
        continue;
      path = buildfilename(basedir, d->d_name, NULL);
      if (access(path, X_OK) == 0) {
        hooks = (char **)xrealloc(hooks, (nhooks + 1) * sizeof(char*));
        hooks[nhooks++] = xstrdup(d->d_name);
      }
      free(path);
    }
    closedir(dp);
    
    /* sort the hooks */
    qsort(hooks, nhooks, sizeof(char *), xstrpcmp);
    
    /* setup the argument vector for the hooks */
    nargv = xmalloc((argc + 2) * sizeof(char *));
    for (i = 0; i < argc; ++i)
      nargv[i + 1] = argv[i];
    nargv[argc + 1] = NULL;

    /* execute the hooks one by one */
    for (n = 0; n < nhooks; ++n) {
      /* figure out the absolute path to the hook */
      path = buildfilename(basedir, hooks[n], NULL);
      
      /* prepare the argv */
      nargv[0] = hooks[n];

      /* reset stdin first */
      if (lseek(STDIN_FILENO, 0, SEEK_SET) < 0)
        xerr(EXIT_FAILURE, "Failed to reset stdin");

      /* fork a new process for this hook */
      pid = fork();
      if (pid < 0) {
        xerr(EXIT_FAILURE, "Failed to spawn new process");
      }
      else if (pid == 0) {
        /* execute the hook script */
        execv(path, nargv);
        fprintf(stderr, "%s: Failed to execute hook %s (%s)\n", progname, path, strerror(errno));
        fflush(stderr);
        _exit(127);
      }
      else {
        /* wait for the child process to terminate */
        while ((child = wait(&status)) != pid) {
          if (child < 0)
            xerr(EXIT_FAILURE, "Failed to wait for %s to terminate", path);
        }
        
        /* check the exit status of the child process */
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
          exit(WEXITSTATUS(status));
        }
        else if (!WIFEXITED(status)) {
          xerrx(EXIT_FAILURE, "Hook %s terminated abnormally", path);
        }
      }
      
      /* cleanup */
      free(hooks[n]);
      free(path);
    }

    /* cleanup */
    free(nargv);
    free(hooks);
  }

  return EXIT_SUCCESS;
}

