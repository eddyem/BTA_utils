/*
 * main.c
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <math.h>
#include "usefull_macros.h"
#include "cmdlnopts.h"
#include "readwfs.h"
#include "readdat.h"
#include "zernike.h"


glob_pars *GP = NULL;

/**
 * Read and dump WFS file
 */
void proc_WFS(){
    int fd = open(GP->inwfs, O_RDONLY);
    if(fd < 0){
        WARN(_("open %s"), GP->inwfs);
        return;
    }
    if(!test_wfs_file(fd)){
        WARNX(_("Bad WFS file %s"), GP->inwfs);
        return;
    }
    Zhistory *hist = show_zhistry(fd);
    print_table(hist, fd);
    hist = show_zhistry(fd);
    print_table(hist, fd);
    close(fd);
}

void proc_DAT(){
    int i, Zn;
    if(fabs(GP->step - DEFAULT_CRD_STEP) > DBL_EPSILON){ // user change default step
        if((i = z_set_step(GP->step)))
            WARNX(_("Can't change step to %g, value is too %s"), GP->step, i < 0 ? "small" : "big");
            return;
    }
    if(fabs(GP->wavelength - DEFAULT_WAVELENGTH) > DBL_EPSILON){ // user want to change wavelength
        // WARNING! This option test should be before changing unit because units depends on wavelength
        if(z_set_wavelength(GP->wavelength)){
            WARNX(_("Bad wavelength: %g, should be from 100 to 10000um (1e-7 to 1e-5 or 0.1 to 10)"), GP->wavelength);
            return;
        }
    }
    if(strcmp(GP->wfunits, DEFAULT_WF_UNIT)){ // user ask to change default unit
        if(z_set_wfunit(GP->wfunits)){
            WARNX(_("Bad wavefront unit: %s. Should be one of"), GP->wfunits);
            z_print_wfunits();
            return;
        }
    }
    double *zerncoeffs = read_dat_file(GP->indat, &Zn);
    if(!zerncoeffs){
        WARNX(_("Bad DAT file %s"), GP->indat);
        return;
    }
    printf("Read coefficients:\n");
    for(i = 0; i < Zn; ++i) printf("%4d\t%g\n", i, zerncoeffs[i]);
}

/**
 * Read DAT file and build table with wavefront coordinates
 */
int main(int argc, char** argv){
    initial_setup();
    GP = parse_args(argc, argv);
    if(!GP->inwfs && !GP->indat) ERRX(_("You should give input file name"));
    if(GP->inwfs){
        proc_WFS();
    }
    if(GP->indat){
        proc_DAT();
    }
    return 0;
}
