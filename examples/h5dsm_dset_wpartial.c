#include "h5dsm_example.h"
#include <time.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    uuid_t pool_uuid;
    char *pool_grp = "daos_tier0";
    hid_t file = -1, dset = -1, file_space = -1, mem_space = -1, trans = -1, fapl = -1;
    hsize_t dims[2] = {4, 6};
    hsize_t start[2], count[2];
    uint64_t trans_num;
    int buf[4][6];
    int rank, mpi_size;
    char *file_sel_str[2] = {"XXX...", "...XXX"};
    int i, j;

    (void)MPI_Init(&argc, &argv);
    (void)daos_init();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if(mpi_size != 2)
        PRINTF_ERROR("mpi_size != 2\n");

    /* Seed random number generator */
    srand(time(NULL));

    if(argc != 4)
        PRINTF_ERROR("argc != 4\n");

    /* Parse UUID */
    if(0 != uuid_parse(argv[1], pool_uuid))
        ERROR;

    /* Set up FAPL */
    if((fapl = H5Pcreate(H5P_FILE_ACCESS)) < 0)
        ERROR;
    if(H5Pset_fapl_daosm(fapl, MPI_COMM_WORLD, MPI_INFO_NULL, pool_uuid, pool_grp) < 0)
        ERROR;

    /* Open file */
    if((file = H5Fopen_ff(argv[2], H5F_ACC_RDWR, fapl, &trans)) < 0)
        ERROR;

    /* Open dataset */
    if((dset = H5Dopen_ff(file, argv[3], H5P_DEFAULT, trans)) < 0)
        ERROR;

    /* Get next transaction */
    if(H5TRget_trans_num(trans, &trans_num) < 0)
        ERROR;
    if(H5TRclose(trans) < 0)
        ERROR;
    if((trans = H5TRcreate(file, trans_num + 1)) < 0)
        ERROR;

    if(rank == 1)
        MPI_Barrier(MPI_COMM_WORLD);

    printf("---------------Rank %d---------------\n", rank);
    printf("Selecting elements denoted with X\n");
    printf("Memory File\n");
    printf("...... %s\n", file_sel_str[rank]);
    for(i = 1; i < 4; i++)
        printf(".XXXXX. %s\n", file_sel_str[rank]);

    /* Fill and print buffer */
    printf("Writing data. Buffer is:\n");
    for(i = 0; i < 4; i++) {
        for(j = 0; j < 6; j++) {
            buf[i][j] = rand() % 10;
            printf("%d ", buf[i][j]);
        }
        printf("\n");
    }

    printf("Transaction number = %llu\n", (long long unsigned)(trans_num + 1));

    /* Set up dataspaces */
    if((file_space = H5Screate_simple(2, dims, NULL)) < 0)
        ERROR;
    if((mem_space = H5Screate_simple(2, dims, NULL)) < 0)
        ERROR;
    start[0] = 0;
    start[1] = 3 * rank;
    count[0] = 4;
    count[1] = 3;
    if(H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, NULL, count, NULL) < 0)
        ERROR;
    start[0] = 1;
    start[1] = 1;
    count[0] = 3;
    count[1] = 4;
    if(H5Sselect_hyperslab(mem_space, H5S_SELECT_SET, start, NULL, count, NULL) < 0)
        ERROR;

    /* Write data */
    if(H5Dwrite_ff(dset, H5T_NATIVE_INT, mem_space, file_space, H5P_DEFAULT, buf, trans) < 0)
        ERROR;

    MPI_Barrier(MPI_COMM_WORLD);

    if(rank == 0)
        if(H5TRcommit(trans) < 0)
            ERROR;

    MPI_Barrier(MPI_COMM_WORLD);

    /* Close */
    if(H5Dclose_ff(dset, -1) < 0)
        ERROR;
    if(H5TRclose(trans) < 0)
        ERROR;
    if(H5Fclose_ff(file, -1) < 0)
        ERROR;
    if(H5Sclose(file_space) < 0)
        ERROR;
    if(H5Sclose(mem_space) < 0)
        ERROR;
    if(H5Pclose(fapl) < 0)
        ERROR;

    printf("Success\n");

    (void)daos_fini();
    (void)MPI_Finalize();
    return 0;

error:
    H5E_BEGIN_TRY {
        H5Dclose_ff(dset, -1);
        H5TRclose(trans);
        H5Fclose_ff(file, -1);
        H5Sclose(file_space);
        H5Sclose(mem_space);
        H5Pclose(fapl);
    } H5E_END_TRY;

    (void)daos_fini();
    (void)MPI_Finalize();
    return 1;
}

