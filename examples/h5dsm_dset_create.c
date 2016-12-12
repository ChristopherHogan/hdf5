#include "h5dsm_example.h"

int main(int argc, char *argv[]) {
    uuid_t pool_uuid;
    char *pool_grp = "daos_tier0";
    hid_t file = -1, dset = -1, trans = -1, space = -1, fapl = -1;
    hsize_t dims[2] = {4, 6};
    uint64_t trans_num;

    (void)MPI_Init(&argc, &argv);
    (void)daos_init();

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

    /* Set up dataspace */
    if((space = H5Screate_simple(2, dims, NULL)) < 0)
        ERROR;

    /* Open file */
    if((file = H5Fopen_ff(argv[2], H5F_ACC_RDWR, fapl, &trans)) < 0)
        ERROR;

    /* Get next transaction */
    if(H5TRget_trans_num(trans, &trans_num) < 0)
        ERROR;
    if(H5TRclose(trans) < 0)
        ERROR;
    if((trans = H5TRcreate(file, trans_num + 1)) < 0)
        ERROR;

    printf("Creating dataset - transaction number = %llu\n", (long long unsigned)(trans_num + 1));

    /* Create dataset */
    if((dset = H5Dcreate_ff(file, argv[3], H5T_NATIVE_INT, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT, trans)) < 0)
        ERROR;

    /* Commit transaction */
    if(H5TRcommit(trans) < 0)
        ERROR;

    /* Close */
    if(H5Dclose_ff(dset, -1) < 0)
        ERROR;
    if(H5TRclose(trans) < 0)
        ERROR;
    if(H5Fclose_ff(file, -1) < 0)
        ERROR;
    if(H5Sclose(space) < 0)
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
        H5Sclose(space);
        H5Pclose(fapl);
    } H5E_END_TRY;

    (void)daos_fini();
    (void)MPI_Finalize();
    return 1;
}

