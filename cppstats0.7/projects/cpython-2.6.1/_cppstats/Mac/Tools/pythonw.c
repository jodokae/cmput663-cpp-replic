#include <unistd.h>
#include <err.h>
static char Python[] = PYTHONWEXECUTABLE;
int main(int argc, char **argv) {
argv[0] = Python;
execv(Python, argv);
err(1, "execv: %s", Python);
}