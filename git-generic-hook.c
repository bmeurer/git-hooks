/**
 * TODO
 *
 * Copyright (c) 2009 Benedikt Meurer <benedikt.meurer@googlemail.com>
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* --- constants --- */
static const char *HOOKNAMES[] =
{
  "applypatch-msg",
  "commit-msg",
  "post-commit",
  "post-receive",
  "post-update",
  "pre-applypatch",
  "pre-commit",
  "pre-rebase",
  "prepare-commit-msg",
  "update",
  NULL
};

static const char *GITITEMS[] =
{
  "HEAD",
  "hooks",
  "info",
  "objects",
  "refs",
  NULL
};


/* --- global variables --- */
static const char *hookname;


static void xerr(int code, const char *format, ...)
{
  const char *errmsg;
  va_list ap;

  errmsg = strerror(errno);
  va_start(ap, format);
  fprintf(stderr, "%s: ", hookname);
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
  fprintf(stderr, "%s: ", hookname);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(ap);
  exit(code);
}


static void *xmalloc(size_t n)
{
  void *p;

  if (n == 0)
    n = 1;
  p = malloc(n);
  if (p == NULL)
    xerr(EXIT_FAILURE, "Failed to allocate %d bytes of memory", (int)n);
  return p;
}


static void *xrealloc(void *p, size_t n)
{
  if (n == 0)
    n = 1;
  p = (p == NULL) ? malloc(n) : realloc(p, n);
  if (p == NULL)
    xerr(EXIT_FAILURE, "Failed to allocate %d bytes of memory", (int)n);
  return p;
}


static void *xstrdup(const char *s)
{
  if (s == NULL)
    return NULL;
  return strcpy((char *)xmalloc(strlen(s) + 1), s);
}


static char *xbuildfilename(const char *s, ...)
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


static char *gitconfig(const char *key)
{
  char buffer[1024];
  char *value = xstrdup("");
  char *t;
  pid_t pid, child;
  int fds[2];
  ssize_t n;
  int status;

  if (pipe(fds) < 0)
    xerr(EXIT_FAILURE, "Failed to setup communication pipe");

  pid = fork();
  if (pid < 0) {
    xerr(EXIT_FAILURE, "Failed to spawn new process");
  }
  else if (pid == 0) {
    close(fds[0]);
    if (dup2(fds[1], STDOUT_FILENO) < 0) {
      fprintf(stderr, "%s: Failed to setup communication pipe\n", hookname);
    }
    else {
      execlp("git", "git", "config", "-z", key, NULL);
      fprintf(stderr, "%s: Failed to execute git (%s)\n", hookname, strerror(errno));
    }
    fflush(stderr);
    _exit(127);
  }
  else {
    close(fds[1]);
    for (;;) {
      n = read(fds[0], buffer, 1024);
      if (n < 0)
        xerr(EXIT_FAILURE, "Failed to read from communication pipe");
      else if (n == 0)
        break;
      value = (char *)xrealloc(value, strlen(value) + n + 1);
      strncat(value, buffer, n);
    }
    close(fds[0]);
    for (;;) {
      child = wait(&status);
      if (child < 0)
        xerr(EXIT_FAILURE, "Failed to wait for git process to terminate");
      if (child == pid)
        break;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      if (WEXITSTATUS(status) == 1) {
        free(value);
        return NULL;
      }
      if (WEXITSTATUS(status) != 127)
        xerrx(WEXITSTATUS(status), "Failed to query git config value %s", key);
      exit(WEXITSTATUS(status));
    }
    else if (!WIFEXITED(status))
      xerrx(EXIT_FAILURE, "git process terminated abnormally");

    /* normalize value */
    for (t = value; isspace(*t); )
      t++;
    for (n = strlen(t); n > 0 && isspace(t[n - 1]); )
      t[--n] = '\0';
    if (*t == 0) {
      free(value);
      value = NULL;
    }
    else {
      memmove(value, t, strlen(t) + 1);
    }
  }

  return value;
}


static int xstrpcmp(const void *s1, const void *s2)
{
  return strcmp(*((const char **) s1), *((const char **) s2));
}


static void xhook(const char *path, char **argv)
{
  pid_t pid, child;
  int status;

  /* reset stdin first */
  if (lseek(STDIN_FILENO, 0, SEEK_SET) < 0)
    xerr(EXIT_FAILURE, "Failed to reset stdin");

  /* fork a new process for this hook */
  pid = fork();
  if (pid < 0) {
    xerr(EXIT_FAILURE, "Failed to spawn new process");
  }
  else if (pid == 0) {
    execv(path, argv);
    fprintf(stderr, "%s: Failed to execute hook %s (%s)\n", hookname, path, strerror(errno));
    fflush(stderr);
    _exit(127);
  }
  else {
    for (;;) {
      child = wait(&status);
      if (child < 0)
        xerr(EXIT_FAILURE, "Failed to wait for %s to terminate", path);
      if (child == pid)
        break;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      exit(WEXITSTATUS(status));
    }
    else if (!WIFEXITED(status)) {
      xerrx(EXIT_FAILURE, "Hook %s terminated abnormally", path);
    }
  }
}


static void sighandler(int signo)
{
  (void)signo;
}


int main(int argc, char **argv) 
{
  const char *gitdir;
  char *path;
  char *basedir;
  char *temp;
  const char *s;
  unsigned n;
  ssize_t i, j, k;
  int fd;
  char buffer[1024];
  char template[] = "/tmp/git-generic-hook.XXXXXX";
  char **hooks = NULL;
  unsigned nhooks = 0;
  struct dirent *d;
  DIR *dp;
  char **nargv;

  /* setup SIGCHLD and SIGPIPE */
  signal(SIGCHLD, sighandler);
  signal(SIGPIPE, sighandler);

  /* figure out the hookname (basename of argv[0]) */
  for (hookname = s = argv[0]; *s != '\0'; ++s)
    if (*s == '/')
      hookname = s + 1;

  /* ensure that the hookname is valid */
  for (n = 0; HOOKNAMES[n] != NULL; ++n)
    if (strcmp(hookname, HOOKNAMES[n]) == 0)
      break;
  if (HOOKNAMES[n] == NULL)
    xerrx(EXIT_FAILURE, "Invalid hook %s", hookname);
  
  /* make sure that $GIT_DIR is set */
  gitdir = getenv("GIT_DIR");
  if (gitdir == NULL)
    xerrx(EXIT_FAILURE, "GIT_DIR is unset");
  
  /* make sure that $GIT_DIR points to a git repository */
  for (n = 0; GITITEMS[n] != NULL; ++n) {
    path = xbuildfilename(gitdir, GITITEMS[n], NULL);
    if (access(path, R_OK) < 0)
      xerr(EXIT_FAILURE, "Failed to access %s", path);
    free(path);
  }

  /* create a temporary file and immediately unlink it */
  fd = mkstemp(template);
  if (fd < 0)
    xerr(EXIT_FAILURE, "Failed to create temporary file");
  if (unlink(template) < 0 && errno != ENOENT)
    xerr(EXIT_FAILURE, "Failed to unlink temporary file");
    
  /* write stdin to the temporary file */
  for (;;) {
    i = read(STDIN_FILENO, buffer, 1024);
    if (i < 0)
      xerr(EXIT_FAILURE, "Failed to read from stdin");
    else if (i == 0)
      break;
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

  /* figure out the hooks.basedir setting (exit if unset) */
  basedir = gitconfig("hooks.basedir");
  if (basedir != NULL) {
    /* make sure, hooks.basedir points to a valid directory */
    dp = opendir(basedir);
    if (dp == NULL)
      xerr(EXIT_FAILURE, "Failed to open directory %s", basedir);
    closedir(dp);

    /* figure out the path to the <hookname>.d directory */
    temp = xmalloc(strlen(hookname) + 3);
    strcpy(temp, hookname);
    strcat(temp, ".d");
    path = xbuildfilename(basedir, temp, NULL);
    free(basedir);
    free(temp);
    basedir = path;

    /* try to read the hooks from the <hookname>.d directory */
    dp = opendir(basedir);
    if (dp != NULL) {
      for (;;) {
        d = readdir(dp);
        if (d == NULL)
          break;
        if (*d->d_name == '.')
          continue;
        path = xbuildfilename(basedir, d->d_name, NULL);
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
      nargv = xmalloc((argc + 1) * sizeof(char*));
      for (i = 1; i < argc; ++i)
        nargv[i] = argv[i];
      nargv[i] = NULL;

      /* execute the hooks one by one */
      for (n = 0; n < nhooks; ++n) {
        /* execute the hook */
        path = xbuildfilename(basedir, hooks[n], NULL);
        nargv[0] = hooks[n];
        xhook(path, nargv);
        free(hooks[n]);
        free(path);
      }
      free(nargv);
      free(hooks);
    }

    free(basedir);
  }

  return EXIT_SUCCESS;
}

