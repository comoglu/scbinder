/* Probe tlay values after processing lay/psratio commands from a file. */
#include <stdio.h>
#include <kom.h>
#include <tlay.h>
int t_com(void);
int main(int argc, char **argv) {
    int n = k_open(argv[1]);
    if (!n) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    while (n > 0) {
        while (k_rd()) {
            char *com = k_str();
            if (!com || com[0]=='#') continue;
            t_com();
        }
        n = k_close();
    }
    double dtdr, dtdz;
    double rs[] = {50, 95.12, 150, 200, 300};
    for (int i = 0; i < 5; i++)
        printf("t_lay(%.2f, 5.0) = %.4f\n", rs[i], t_lay(rs[i], 5.0, &dtdr, &dtdz));
    printf("t_phase(P,95.12,5) = %.4f\n", t_phase(0, 95.12, 5.0, &dtdr, &dtdz));

    return 0;
}
