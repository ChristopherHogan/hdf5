/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "H5VLiod_server.h"

#ifdef H5_HAVE_EFF

/*
 * Programmer:  Mohamad Chaarawi <chaarawi@hdfgroup.gov>
 *              June, 2013
 *
 * Purpose:	The IOD plugin server side attribute routines.
 */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_create_cb
 *
 * Purpose:	Creates a attr as a iod object.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_create_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                               size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                               size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                               void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_create_in_t *input = (attr_create_in_t *)op_data->input;
    attr_create_out_t output;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start lookup */
    iod_obj_id_t loc_id = input->loc_id; /* The ID of the current location object */
    iod_obj_id_t loc_attrkv_id = input->loc_attrkv_id; /* The ID of the attribute KV of the location object*/
    iod_obj_id_t attr_id = input->attr_id; /* The ID of the attribute that needs to be created */
    iod_obj_id_t mdkv_id = input->mdkv_id; /* The ID of the metadata KV to be created */
    iod_trans_id_t wtid = input->trans_num;
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handles_t attr_oh, obj_oh; /* object handles */
    iod_handle_t attr_kv_oh, mdkv_oh;
    iod_obj_id_t obj_id;
    const char *loc_name = input->path; /* path to start hierarchy traversal */
    const char *attr_name = input->attr_name; /* attribute's name */
    iod_array_struct_t array; /* IOD array structure for attribute's creation */
    iod_size_t *max_dims; /* MAX dims for IOD */
    scratch_pad sp;
    iod_checksum_t sp_cs = 0;
    iod_size_t array_dims[H5S_MAX_RANK], current_dims[H5S_MAX_RANK];
    iod_hint_list_t *obj_create_hint = NULL;
    hbool_t opened_locally = FALSE;
    iod_ret_t ret;
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute create %s at %"PRIu64" with ID %"PRIx64" %"PRIx64"\n",
            attr_name, loc_handle.wr_oh.cookie, attr_id, loc_attrkv_id);
#endif

    attr_oh.rd_oh.cookie = IOD_OH_UNDEFINED;
    attr_oh.wr_oh.cookie = IOD_OH_UNDEFINED;
    mdkv_oh.cookie = IOD_OH_UNDEFINED;
    attr_kv_oh.cookie = IOD_OH_UNDEFINED;

    if(cs_scope & H5_CHECKSUM_IOD) {
        obj_create_hint = (iod_hint_list_t *)malloc(sizeof(iod_hint_list_t) + sizeof(iod_hint_t));
        obj_create_hint->num_hint = 1;
        obj_create_hint->hint[0].key = "iod_hint_obj_enable_cksum";
    }

    if(loc_handle.rd_oh.cookie == IOD_OH_UNDEFINED) {
        /* Try and open the starting location */
        ret = iod_obj_open_read(coh, loc_id, wtid, NULL, &loc_handle.rd_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open start location");
        opened_locally = TRUE;
    }

    /* Open the object where the attribute needs to be created. */
    ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, rtid, 
                                    cs_scope, &obj_id, &obj_oh);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open object");

    /* Set the IOD array creation parameters */
    array.cell_size = (uint32_t)H5Tget_size(input->type_id);
    array.num_dims = (uint32_t)H5Sget_simple_extent_ndims(input->space_id);

    /* Handle Scalar Dataspaces (set rank and current dims size to 1) */
    if(0 == array.num_dims) {
        array.num_dims = 1;
        array.firstdim_max = 1;
        current_dims[0] = 1;
    }
    else {
        if(H5Sget_simple_extent_dims(input->space_id, current_dims, array_dims) < 0)
            HGOTO_ERROR_FF(FAIL, "can't get dimentions' sizes");
        array.firstdim_max = array_dims[0];
    }

    array.current_dims = current_dims;
    array.chunk_dims = NULL;

    /* create the attribute */
    ret = iod_obj_create(coh, wtid, obj_create_hint, IOD_OBJ_ARRAY, NULL, 
                         &array, &attr_id, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't create Attribute");

    ret = iod_obj_open_read(coh, attr_id, wtid, NULL, &attr_oh.rd_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open attribute");
    ret = iod_obj_open_write(coh, attr_id, wtid, NULL, &attr_oh.wr_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open attribute");

    /* create the metadata KV object for the attribute */
    ret = iod_obj_create(coh, wtid, obj_create_hint, IOD_OBJ_KV, NULL, NULL, &mdkv_id, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't create metadata KV object");

    /* set values for the scratch pad object */
    sp[0] = mdkv_id;
    sp[1] = IOD_OBJ_INVALID;
    sp[2] = IOD_OBJ_INVALID;
    sp[3] = IOD_OBJ_INVALID;

    /* set scratch pad in attribute */
    if(cs_scope & H5_CHECKSUM_IOD) {
        sp_cs = H5_checksum_crc64(&sp, sizeof(sp));
        ret = iod_obj_set_scratch(attr_oh.wr_oh, wtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't set scratch pad");
    }
    else {
        ret = iod_obj_set_scratch(attr_oh.wr_oh, wtid, &sp, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't set scratch pad");
    }

    /* Open Metadata KV object for write */
    ret = iod_obj_open_write(coh, mdkv_id, wtid, NULL, &mdkv_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't create scratch pad");

    /* insert object type metadata */
    ret = H5VL_iod_insert_object_type(mdkv_oh, wtid, H5I_ATTR, cs_scope, NULL, NULL);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't insert KV value");

    /* MSC - need to check size of datatype if it fits in
       entry otherwise create a BLOB */

    /* insert datatype metadata */
    ret = H5VL_iod_insert_datatype(mdkv_oh, wtid, input->type_id, cs_scope, NULL, NULL);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't insert KV value");

    /* insert dataspace metadata */
    ret = H5VL_iod_insert_dataspace(mdkv_oh, wtid, input->space_id, cs_scope, NULL, NULL);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't insert KV value");

    /* if the starting location is not the last component, need to
       read the attrkv_id of the last object where attribute needs
       to be created */
    if(loc_id != obj_id || loc_attrkv_id == IOD_OBJ_INVALID) {
        /* get scratch pad of the parent */
        ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* open the attribute KV in scratch pad */
        ret = iod_obj_open_write(coh, sp[1], wtid, NULL /*hints*/, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }
    else {
        /* open the attribute KV */
        ret = iod_obj_open_write(coh, loc_attrkv_id, wtid, NULL, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }

    /* insert new attribute in scratch pad of current object */
    ret = H5VL_iod_insert_new_link(attr_kv_oh, wtid, attr_name, 
                                   H5L_TYPE_HARD, &attr_id, cs_scope, NULL, NULL);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't insert KV value");

    output.iod_oh.rd_oh.cookie = attr_oh.rd_oh.cookie;
    output.iod_oh.wr_oh.cookie = attr_oh.wr_oh.cookie;

#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr create, sending response to client\n");
#endif

    HG_Handler_start_output(op_data->hg_handle, &output);

done:

    /* close parent group if it is not the location we started the
       traversal into */
    if(TRUE == opened_locally || loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        iod_obj_close(obj_oh.rd_oh, NULL, NULL);
    }

    iod_obj_close(mdkv_oh, NULL, NULL);
    iod_obj_close(attr_kv_oh, NULL, NULL);

    /* return an UNDEFINED oh to the client if the operation failed */
    if(ret_value < 0) {
        fprintf(stderr, "Failed Attribute Create\n");

        if(attr_oh.rd_oh.cookie != IOD_OH_UNDEFINED &&
           iod_obj_close(attr_oh.rd_oh, NULL, NULL) < 0)
            HDONE_ERROR_FF(FAIL, "can't close object");
        if(attr_oh.wr_oh.cookie != IOD_OH_UNDEFINED &&
           iod_obj_close(attr_oh.wr_oh, NULL, NULL) < 0)
            HDONE_ERROR_FF(FAIL, "can't close object");

        output.iod_oh.rd_oh.cookie = IOD_OH_UNDEFINED;
        output.iod_oh.wr_oh.cookie = IOD_OH_UNDEFINED;
        HG_Handler_start_output(op_data->hg_handle, &output);
    }

    if(obj_create_hint) {
        free(obj_create_hint);
        obj_create_hint = NULL;
    }

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);

    input = (attr_create_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_create_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_open_cb
 *
 * Purpose:	Opens a attribute as a iod object.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_open_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                             size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                             size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                             void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_open_in_t *input = (attr_open_in_t *)op_data->input;
    attr_open_out_t output;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start traversal */
    iod_obj_id_t loc_id = input->loc_id; /* location ID */
    iod_obj_id_t loc_attrkv_id = input->loc_attrkv_id; /* The ID of the attribute KV of the location object*/
    const char *loc_name = input->path; /* current  path to start traversal */
    const char *attr_name = input->attr_name; /* attribute's name to open */
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handles_t attr_oh, obj_oh;
    iod_handle_t attr_kv_oh, mdkv_oh;
    iod_obj_id_t obj_id;
    iod_obj_id_t attr_id;
    scratch_pad sp;
    iod_checksum_t sp_cs = 0;
    H5VL_iod_link_t iod_link;
    iod_ret_t ret;
    herr_t ret_value = SUCCEED;

    output.space_id = FAIL;
    output.type_id = FAIL;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute open %s at %s (OH %"PRIu64" ID %"PRIx64")\n", 
            attr_name, loc_name, loc_handle.rd_oh.cookie, loc_id);
#endif

    /* Open the object where the attribute needs to be opened. */
    ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, 
                                    rtid, cs_scope, &obj_id, &obj_oh);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't open object");

#if H5_EFF_DEBUG
    fprintf(stderr, "Attribute is on object (OH %"PRIu64" ID %"PRIx64")\n", 
            obj_oh.rd_oh.cookie, obj_id);
#endif

    /* if the starting location is not the last component, need to
       read the attrkv_id of the last object where attribute needs
       to be created */
    if(loc_id != obj_id || loc_attrkv_id == IOD_OBJ_INVALID) {
        /* get scratch pad of the object */
        ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* if attribute KV does not exist, return error*/
        if(IOD_OBJ_INVALID == sp[1])
            HGOTO_ERROR_FF(FAIL, "Object has no attributes");

        /* open the attribute KV in scratch pad */
        ret = iod_obj_open_read(coh, sp[1], rtid, NULL, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }
    else {
        /* open the attribute KV */
        ret = iod_obj_open_read(coh, loc_attrkv_id, rtid, NULL, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }

    /* get attribute ID */
    ret = H5VL_iod_get_metadata(attr_kv_oh, rtid, H5VL_IOD_LINK, 
                                attr_name, cs_scope, NULL, &iod_link);
    if(ret != SUCCEED)
        HGOTO_ERROR_FF(ret, "can't retrieve Attribute ID from parent KV store");

    HDassert(iod_link.link_type == H5L_TYPE_HARD);
    attr_id = iod_link.u.iod_id;

    /* close parent group if it is not the location we started the
       traversal into */
    if(loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        ret = iod_obj_close(obj_oh.rd_oh, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't close IOD object");
    }

    /* close the attribute KV holder */
    ret = iod_obj_close(attr_kv_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close IOD object");

    /* open the attribute */
    ret = iod_obj_open_read(coh, attr_id, rtid, NULL, &attr_oh.rd_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open current group");
    ret = iod_obj_open_write(coh, attr_id, rtid, NULL, &attr_oh.wr_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open current group");

    /* get scratch pad of the attribute */
    ret = iod_obj_get_scratch(attr_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

    if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
        /* verify scratch pad integrity */
        if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
            HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
    }

    /* open the metadata scratch pad of the attribute */
    ret = iod_obj_open_read(coh, sp[0], rtid, NULL, &mdkv_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open scratch pad");

    ret = H5VL_iod_get_metadata(mdkv_oh, rtid, H5VL_IOD_DATATYPE, 
                                H5VL_IOD_KEY_OBJ_DATATYPE,
                                cs_scope, NULL, &output.type_id);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "failed to retrieve datatype");

    ret = H5VL_iod_get_metadata(mdkv_oh, rtid, H5VL_IOD_DATASPACE, 
                                H5VL_IOD_KEY_OBJ_DATASPACE,
                                cs_scope, NULL, &output.space_id);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "failed to retrieve dataspace");

    /* close the metadata scratch pad */
    ret = iod_obj_close(mdkv_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

    output.iod_id = attr_id;
    output.mdkv_id = sp[0];
    output.iod_oh.rd_oh.cookie = attr_oh.rd_oh.cookie;
    output.iod_oh.wr_oh.cookie = attr_oh.wr_oh.cookie;

#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr open, sending response to client\n");
#endif

    HG_Handler_start_output(op_data->hg_handle, &output);

done:

    if(FAIL != output.type_id)
        H5Tclose(output.type_id);
    if(FAIL != output.space_id)
        H5Sclose(output.space_id);

    if(ret_value < 0) {
        output.iod_oh.rd_oh.cookie = IOD_OH_UNDEFINED;
        output.iod_oh.wr_oh.cookie = IOD_OH_UNDEFINED;
        output.iod_id = IOD_OBJ_INVALID;
        output.space_id = FAIL;
        output.type_id = FAIL;
        HG_Handler_start_output(op_data->hg_handle, &output);
    }

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_open_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_open_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_read_cb
 *
 * Purpose:	Reads from IOD into the function shipper BDS handle.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_read_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                             size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                             size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                             void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_io_in_t *input = (attr_io_in_t *)op_data->input;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handle_t iod_oh = input->iod_oh.rd_oh; /* attribute's object handle */
    iod_obj_id_t iod_id = input->iod_id; /* attribute's ID */
    iod_obj_id_t mdkv_id = input->mdkv_id; /* The ID of the metadata KV */
    hg_bulk_t bulk_handle = input->bulk_handle; /* bulk handle for data */
    //hid_t type_id = input->type_id; /* datatype ID of data */
    hid_t space_id = input->space_id; /* dataspace of attribute */
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handle_t mdkv_oh;
    hg_bulk_t bulk_block_handle; /* HG block handle */
    hg_bulk_request_t bulk_request; /* HG request */
    iod_mem_desc_t *mem_desc = NULL; /* memory descriptor used for reading array */
    iod_array_iodesc_t file_desc; /* file descriptor used to read array */
    iod_hyperslab_t hslabs; /* IOD hyperslab generated from HDF5 filespace */
    size_t size; /* size of outgoing bulk data */
    void *buf; /* buffer to hold outgoing data */
    iod_checksum_t iod_cs = 0, attr_cs = 0;
    int ndims; /* dataset's rank/number of dimensions */
    hssize_t num_descriptors = 0; /* number of IOD file descriptors needed to describe filespace selection */
    na_addr_t dest = HG_Handler_get_addr(op_data->hg_handle); /* destination address to push data to */
    na_class_t *na_class = HG_Handler_get_na_class(op_data->hg_handle); /* NA transfer class */
    na_bool_t is_coresident = NA_Addr_is_self(na_class, dest);
    hbool_t opened_locally = FALSE; /* flag to indicate whether we opened the attribute here or if it was already open */
    iod_ret_t ret;
    herr_t ret_value = SUCCEED;

    /* open the attribute if we don't have the handle yet */
    if(iod_oh.cookie == IOD_OH_UNDEFINED) {
        ret = iod_obj_open_read(coh, iod_id, rtid, NULL, &iod_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open current group");        
        opened_locally = TRUE;
    }

#if H5_EFF_DEBUG 
    fprintf(stderr, "Start Attribute Read on OH %"PRIu64" OID %"PRIx64"\n", 
            iod_oh.cookie, iod_id);
#endif

    size = HG_Bulk_handle_get_size(bulk_handle);

    if(is_coresident) {
        size_t bulk_size = 0;

        bulk_block_handle = bulk_handle;
        /* get  mercury buffer where data is */
        if(HG_SUCCESS != HG_Bulk_handle_access(bulk_block_handle, 0, size,
                                               HG_BULK_READWRITE, 1, &buf, &bulk_size, NULL))
            HGOTO_ERROR_FF(FAIL, "Could not access handle");

        assert(size == bulk_size);
    }
    else {
        if(NULL == (buf = malloc(size)))
            HGOTO_ERROR_FF(FAIL, "can't allocate read buffer");

        /* Create bulk handle */
        if(HG_SUCCESS != HG_Bulk_handle_create(1, &buf, &size, 
                                               HG_BULK_READWRITE, &bulk_block_handle))
            HGOTO_ERROR_FF(FAIL, "can't create bulk handle");
    }

    /* Get dataspace if it is not available */
    if(H5I_UNINIT == space_id) {
        /* open the metadata scratch pad of the attribute */
        ret = iod_obj_open_read(coh, mdkv_id, rtid, NULL, &mdkv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");

        ret = H5VL_iod_get_metadata(mdkv_oh, rtid, H5VL_IOD_DATASPACE, 
                                    H5VL_IOD_KEY_OBJ_DATASPACE,
                                    cs_scope, NULL, &space_id);
        if(SUCCEED != ret)
            HGOTO_ERROR_FF(ret, "failed to retrieve dataspace");

        /* close the metadata scratch pad */
        ret = iod_obj_close(mdkv_oh, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't close object");
    }

    /* set the memory descriptor */
    mem_desc = (iod_mem_desc_t *)malloc(sizeof(iod_mem_desc_t) + sizeof(iod_mem_frag_t));
    mem_desc->nfrag = 1;
    mem_desc->frag[0].addr = buf;
    mem_desc->frag[0].len = (iod_size_t)size;

    num_descriptors = 1;

    /* get the rank of the dataspace */
    if((ndims = H5Sget_simple_extent_ndims(space_id)) < 0)
        HGOTO_ERROR_FF(FAIL, "unable to get dataspace dimesnsion");

    /* handle scalar dataspace */
    if(0 == ndims) {
        ndims = 1;

        hslabs.start = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.stride = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.block = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.count = (iod_size_t *)malloc(sizeof(iod_size_t));

        num_descriptors = 1;
        hslabs.start[0] = 0;
        hslabs.count[0] = 1;
        hslabs.block[0] = 1;
        hslabs.stride[0] = 1;
    }
    else {
        hslabs.start = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.stride = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.block = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.count = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);

        /* generate the descriptor */
        if(H5VL_iod_get_file_desc(space_id, &num_descriptors, &hslabs) < 0)
            HGOTO_ERROR_FF(FAIL, "unable to generate IOD file descriptor from dataspace selection");
    }

    /* set the file descriptor */
    file_desc = hslabs;

    /* read from array object */
    if(cs_scope & H5_CHECKSUM_IOD) {
        ret = iod_array_read(iod_oh, rtid, NULL, mem_desc, &file_desc, 
                             &iod_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't read from array object");

        attr_cs = H5_checksum_crc64(buf, size);

        if(attr_cs != iod_cs)
            HGOTO_ERROR_FF(FAIL, "Data corruption detected when reading attribute");
    }
    else {
        ret = iod_array_read(iod_oh, rtid, NULL, mem_desc, &file_desc, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't read from array object");
    }

    if(!is_coresident) {
        /* Push data to the client */
        if(HG_SUCCESS != HG_Bulk_transfer(HG_BULK_PUSH, dest, bulk_handle, 0, 
                                          bulk_block_handle, 0, size, &bulk_request))
            HGOTO_ERROR_FF(FAIL, "Transfer data failed");

        /* Wait for bulk data read to complete */
        if(HG_SUCCESS != HG_Bulk_wait(bulk_request, HG_MAX_IDLE_TIME, HG_STATUS_IGNORE))
            HGOTO_ERROR_FF(FAIL, "can't wait for bulk data operation");
    }

done:
#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr read, sending response to client\n");
#endif

    if(HG_SUCCESS != HG_Handler_start_output(op_data->hg_handle, &ret_value))
        HDONE_ERROR_FF(FAIL, "can't send result of write to client");

    if(!is_coresident) {
        /* free block handle */
        if(HG_SUCCESS != HG_Bulk_handle_free(bulk_block_handle))
            HGOTO_ERROR_FF(FAIL, "can't free bds block handle");
        if(buf) {
            free(buf);
            buf = NULL;
        }
    }

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_io_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

    /* free allocated descriptors */
    free(hslabs.start);
    free(hslabs.stride);
    free(hslabs.block);
    free(hslabs.count);
    if(mem_desc)
        free(mem_desc);

    /* close the attribute if we opened it in this routine */
    if(opened_locally) {
        ret = iod_obj_close(iod_oh, NULL, NULL);
        if(ret < 0)
            HDONE_ERROR_FF(ret, "can't close Array object");
    }
} /* end H5VL_iod_server_attr_read_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_write_cb
 *
 * Purpose:	Writes from IOD into the function shipper BDS handle.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_write_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                              size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                              size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                              void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_io_in_t *input = (attr_io_in_t *)op_data->input;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handle_t iod_oh = input->iod_oh.wr_oh; /* attribute's object handle */
    iod_obj_id_t iod_id = input->iod_id; /* attribute's ID */
    iod_obj_id_t mdkv_id = input->mdkv_id; /* The ID of the metadata KV */
    hg_bulk_t bulk_handle = input->bulk_handle; /* bulk handle for data */
    //hid_t type_id = input->type_id; /* datatype ID of data */
    hid_t space_id = input->space_id; /* dataspace of attribute */
    iod_trans_id_t wtid = input->trans_num;
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handle_t mdkv_oh;
    hg_bulk_t bulk_block_handle; /* HG block handle */
    hg_bulk_request_t bulk_request; /* HG request */
    iod_mem_desc_t *mem_desc = NULL; /* memory descriptor used for writing array */
    iod_array_iodesc_t file_desc; /* file descriptor used to write array */
    iod_hyperslab_t hslabs; /* IOD hyperslab generated from HDF5 filespace */
    size_t size; /* size of outgoing bulk data */
    void *buf; /* buffer to hold outgoing data */
    int ndims; /* dataset's rank/number of dimensions */
    iod_checksum_t attr_cs = 0;
    hssize_t num_descriptors = 0; /* number of IOD file descriptors needed to describe filespace selection*/
    na_addr_t source = HG_Handler_get_addr(op_data->hg_handle); /* source address to pull data from */
    na_class_t *na_class = HG_Handler_get_na_class(op_data->hg_handle); /* NA transfer class */
    na_bool_t is_coresident = NA_Addr_is_self(na_class, source);
    hbool_t opened_locally = FALSE; /* flag to indicate whether we opened the attribute here or if it was already opened */
    iod_ret_t ret;
    herr_t ret_value = SUCCEED;

    /* open the attribute if we don't have the handle yet */
    if(iod_oh.cookie == IOD_OH_UNDEFINED) {
        ret = iod_obj_open_write(coh, iod_id, wtid, NULL /*hints*/, &iod_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open current group");
        opened_locally = TRUE;
    }

#if H5_EFF_DEBUG 
    fprintf(stderr, "Start Attribute Write on OH %"PRIu64" OID %"PRIx64"\n", 
            iod_oh.cookie, iod_id);
#endif

    /* Read bulk data here and wait for the data to be here  */
    size = HG_Bulk_handle_get_size(bulk_handle);

    if(is_coresident) {
        size_t bulk_size = 0;

        bulk_block_handle = bulk_handle;
        /* get  mercury buffer where data is */
        if(HG_SUCCESS != HG_Bulk_handle_access(bulk_block_handle, 0, size,
                                               HG_BULK_READWRITE, 1, &buf, &bulk_size, NULL))
            HGOTO_ERROR_FF(FAIL, "Could not access handle");

        assert(size == bulk_size);
    }
    else {
        if(NULL == (buf = malloc(size)))
            HGOTO_ERROR_FF(FAIL, "can't allocate read buffer");

        /* Create bulk handle */
        if(HG_SUCCESS != HG_Bulk_handle_create(1, &buf, &size, 
                                               HG_BULK_READWRITE, &bulk_block_handle))
            HGOTO_ERROR_FF(FAIL, "can't create bulk handle");

        /* Pull data from the client */
        if(HG_SUCCESS != HG_Bulk_transfer(HG_BULK_PULL, source, bulk_handle, 0, 
                                          bulk_block_handle, 0, size, &bulk_request))
            HGOTO_ERROR_FF(FAIL, "Transfer data failed");

        /* Wait for bulk data read to complete */
        if(HG_SUCCESS != HG_Bulk_wait(bulk_request, HG_MAX_IDLE_TIME, HG_STATUS_IGNORE))
            HGOTO_ERROR_FF(FAIL, "can't wait for bulk data operation");
    }

    /* Get dataspace if it is not available */
    if(H5I_UNINIT == space_id) {
        /* open the metadata scratch pad of the attribute */
        ret = iod_obj_open_read(coh, mdkv_id, wtid, NULL, &mdkv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");

        ret = H5VL_iod_get_metadata(mdkv_oh, rtid, H5VL_IOD_DATASPACE, H5VL_IOD_KEY_OBJ_DATASPACE,
                                    cs_scope, NULL, &space_id);
        if(SUCCEED != ret)
            HGOTO_ERROR_FF(ret, "failed to retrieve dataspace");

        /* close the metadata scratch pad */
        ret = iod_obj_close(mdkv_oh, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't close object");
    }

    /* set the memory descriptor */
    mem_desc = (iod_mem_desc_t *)malloc(sizeof(iod_mem_desc_t) + sizeof(iod_mem_frag_t));
    mem_desc->nfrag = 1;
    mem_desc->frag[0].addr = buf;
    mem_desc->frag[0].len = (iod_size_t)size;

    num_descriptors = 1;

    /* get the rank of the dataspace */
    if((ndims = H5Sget_simple_extent_ndims(space_id)) < 0)
        HGOTO_ERROR_FF(FAIL, "unable to get dataspace dimesnsion");

    /* handle scalar dataspace */
    if(0 == ndims) {
        ndims = 1;

        hslabs.start = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.stride = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.block = (iod_size_t *)malloc(sizeof(iod_size_t));
        hslabs.count = (iod_size_t *)malloc(sizeof(iod_size_t));

        num_descriptors = 1;
        hslabs.start[0] = 0;
        hslabs.count[0] = 1;
        hslabs.block[0] = 1;
        hslabs.stride[0] = 1;
    }
    else {
        hslabs.start = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.stride = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.block = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);
        hslabs.count = (iod_size_t *)malloc(sizeof(iod_size_t) * (size_t)ndims);

        /* generate the descriptor */
        if(H5VL_iod_get_file_desc(space_id, &num_descriptors, &hslabs) < 0)
            HGOTO_ERROR_FF(FAIL, "unable to generate IOD file descriptor from dataspace selection");
    }

    /* set the file descriptor */
    file_desc = hslabs;

    if(cs_scope & H5_CHECKSUM_IOD) {
        attr_cs = H5_checksum_crc64(buf, size);
        /* write from array object */
        ret = iod_array_write(iod_oh, wtid, NULL, mem_desc, &file_desc, &attr_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't write to array object");
    }
    else {
        /* write from array object */
        ret = iod_array_write(iod_oh, wtid, NULL, mem_desc, &file_desc, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't write to array object");
    }

done:
#if H5_EFF_DEBUG 
    fprintf(stderr, "Done with attr write, sending %d response to client\n", ret_value);
#endif

    if(HG_SUCCESS != HG_Handler_start_output(op_data->hg_handle, &ret_value))
        HDONE_ERROR_FF(FAIL, "can't send result of write to client");

    if(!is_coresident) {
        /* free block handle */
        if(HG_SUCCESS != HG_Bulk_handle_free(bulk_block_handle))
            HGOTO_ERROR_FF(FAIL, "can't free bds block handle");

        if(buf) {
            free(buf);
            buf = NULL;
        }
    }

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_io_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

    /* free allocated descriptors */
    free(hslabs.start);
    free(hslabs.stride);
    free(hslabs.block);
    free(hslabs.count);
    if(mem_desc)
        free(mem_desc);

    /* close the attribute if we opened it in this routine */
    if(opened_locally) {
        ret = iod_obj_close(iod_oh, NULL, NULL);
        if(ret < 0)
            HDONE_ERROR_FF(ret, "can't close Array object");
    }
} /* end H5VL_iod_server_attr_write_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_exists_cb
 *
 * Purpose:	Checks if an attribute exists on object.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_exists_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                               size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                               size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                               void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_op_in_t *input = (attr_op_in_t *)op_data->input;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start lookup */
    iod_obj_id_t loc_id = input->loc_id; /* The ID of the current location object */
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handles_t obj_oh; /* current object handle accessed */
    iod_handle_t attr_kv_oh; /* KV handle holding attributes for object */
    iod_obj_id_t obj_id;
    const char *loc_name = input->path; /* path to start hierarchy traversal */
    const char *attr_name = input->attr_name; /* attribute's name */
    scratch_pad sp;
    iod_checksum_t sp_cs = 0;
    iod_size_t kv_size = 0;
    htri_t ret = -1;
    iod_ret_t iod_ret;
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute Exists %s/%s on CV %d\n", loc_name, attr_name, (int)rtid);
#endif

    /* Open the object where the attribute needs to be checked. */
    iod_ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, rtid, 
                                    cs_scope, &obj_id, &obj_oh);
    if(SUCCEED != iod_ret)
        HGOTO_ERROR_FF(iod_ret, "can't open object");

    if(loc_id != obj_id || IOD_OBJ_INVALID == input->loc_attrkv_id) {
        /* get scratch pad of the parent */
        iod_ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(iod_ret < 0)
            HGOTO_ERROR_FF(iod_ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* if attribute KV does not exist, return false*/
        if(IOD_OBJ_INVALID == sp[1]) {
            ret = FALSE;
            HGOTO_DONE(SUCCEED);
        }

        /* open the attribute KV in scratch pad */
        iod_ret = iod_obj_open_read(coh, sp[1], rtid, NULL, &attr_kv_oh, NULL);
        if(iod_ret < 0)
            HGOTO_ERROR_FF(iod_ret, "can't open scratch pad");
    }
    else {

        /* if attribute KV does not exist, return false*/
        if(IOD_OBJ_INVALID == input->loc_attrkv_id) {
	  printf("invalid \n");
	  //ret = FALSE;
	  //HGOTO_DONE(SUCCEED);
        }



        /* open the attribute KV  */
        iod_ret = iod_obj_open_read(coh, input->loc_attrkv_id, rtid, NULL, &attr_kv_oh, NULL);
        if(iod_ret < 0)
            HGOTO_ERROR_FF(iod_ret, "can't open scratch pad");
    }

    /* close parent group if it is not the location we started the
       traversal into */
    if(loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        iod_ret = iod_obj_close(obj_oh.rd_oh, NULL, NULL);
        if(iod_ret < 0)
            HGOTO_ERROR_FF(iod_ret, "can't close group");
    }

    /* get attribute ID */
    if(iod_kv_get_value(attr_kv_oh, rtid, attr_name, (iod_size_t)strlen(attr_name) + 1, 
                        NULL, &kv_size, NULL, NULL) < 0) {
        ret = FALSE;
    }
    else {
        ret = TRUE;
    }

    iod_ret = iod_obj_close(attr_kv_oh, NULL, NULL);
    if(iod_ret < 0)
        HGOTO_ERROR_FF(iod_ret, "can't close group");

done:
#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr exists, sending %d to client\n", ret);
#endif

    HG_Handler_start_output(op_data->hg_handle, &ret);

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_op_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_exists_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_iterate_cb
 *
 * Purpose:	gather all attribute names on an object before iterating 
 *              through them at the client.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              December, 2015
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_iterate_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                               size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                               size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                               void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_op_in_t *input = (attr_op_in_t *)op_data->input;
    attr_iterate_t output;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start lookup */
    iod_obj_id_t loc_id = input->loc_id; /* The ID of the current location object */
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_handles_t obj_oh; /* current object handle accessed */
    iod_handle_t attr_kv_oh; /* KV handle holding attributes for object */
    iod_obj_id_t obj_id;
    const char *loc_name = input->path; /* path to start hierarchy traversal */
    scratch_pad sp;
    iod_checksum_t sp_cs = 0;
    int num_attrs, i;
    iod_ret_t ret;
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute Iterate %s/%s on CV %d\n", loc_name, attr_name, (int)rtid);
#endif

    /* Open the object where the attribute needs to be checked. */
    ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, 
                                    rtid, cs_scope, &obj_id, &obj_oh);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't open object");

    if(loc_id != obj_id || IOD_OBJ_INVALID == input->loc_attrkv_id) {
        /* get scratch pad of the parent */
        ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* if attribute KV does not exist, return error*/
        if(IOD_OBJ_INVALID == sp[1])
            HGOTO_ERROR_FF(FAIL, "Object has no attributes");

        /* open the attribute KV in scratch pad */
        ret = iod_obj_open_read(coh, sp[1], rtid, NULL, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
    }
    else {
        /* open the attribute KV  */
        ret = iod_obj_open_read(coh, input->loc_attrkv_id, rtid, NULL, &attr_kv_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }

    ret = iod_kv_get_num(attr_kv_oh, rtid, &num_attrs, NULL);
    if(ret != 0)
        HGOTO_ERROR_FF(FAIL, "can't get number of KV entries");
 
    output.num_attrs = num_attrs;
    output.attr_names = NULL;

    if(0 != num_attrs) {
        iod_kv_params_t *kvs = NULL;
        iod_kv_t *kv = NULL;
        iod_checksum_t *oid_cs = NULL;
        iod_ret_t *oid_ret = NULL;

        output.attr_names = (const char **)malloc(sizeof(char *) * num_attrs);

        kvs = (iod_kv_params_t *)malloc(sizeof(iod_kv_params_t) * (size_t)num_attrs);
        kv = (iod_kv_t *)malloc(sizeof(iod_kv_t) * (size_t)num_attrs);
        oid_cs = (iod_checksum_t *)malloc(sizeof(iod_checksum_t) * (size_t)num_attrs);
        oid_ret = (iod_ret_t *)malloc(sizeof(iod_ret_t) * (size_t)num_attrs);

        for(i=0 ; i<num_attrs ; i++) {
            kv[i].key = malloc(IOD_KV_KEY_MAXLEN);
            kv[i].key_len = IOD_KV_KEY_MAXLEN;
            kvs[i].kv = &kv[i];
            kvs[i].cs = &oid_cs[i];
            kvs[i].ret = &oid_ret[i];
        }

        ret = iod_kv_list_key(attr_kv_oh, rtid, NULL, 0, &num_attrs, kvs, NULL);
        if(ret != 0)
            HGOTO_ERROR_FF(FAIL, "can't get list of keys");

        for(i=0 ; i<num_attrs ; i++) {
            output.attr_names[i] = strdup((char *)kv[i].key);
            free(kv[i].key);
        }

        free(kv);
        free(oid_cs);
        free(oid_ret);
        free(kvs);
    }

    /* close parent group if it is not the location we started the
       traversal into */
    if(loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        ret = iod_obj_close(obj_oh.rd_oh, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't close group");
    }

    ret = iod_obj_close(attr_kv_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close group");

done:

    output.ret = ret_value;

#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr iterate, sending %d to client\n", ret);
#endif

    HG_Handler_start_output(op_data->hg_handle, &output);

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);

    for(i=0 ; i<num_attrs ; i++)
        free(output.attr_names[i]);
    free(output.attr_names);

    input = (attr_op_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_iterate_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_rename_cb
 *
 * Purpose:	Renames iod HDF5 attribute.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_rename_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                               size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                               size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                               void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_rename_in_t *input = (attr_rename_in_t *)op_data->input;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start lookup */
    iod_obj_id_t loc_id = input->loc_id; /* The ID of the current location object */
    iod_handles_t obj_oh; /* current object handle accessed */
    iod_handles_t attr_kv_oh; /* KV handle holding attributes for object */
    iod_obj_id_t obj_id, attr_id;
    const char *loc_name = input->path; /* path to start hierarchy traversal */
    const char *old_name = input->old_attr_name;
    const char *new_name = input->new_attr_name;
    iod_trans_id_t wtid = input->trans_num;
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_kv_params_t kvs; /* KV lists for objects - used to unlink attribute object */
    iod_kv_t kv; /* KV entry */
    H5VL_iod_link_t iod_link;
    scratch_pad sp;
    iod_ret_t ret;
    iod_checksum_t cs;
    iod_checksum_t sp_cs = 0;
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute Rename %s to %s\n", old_name, new_name);
#endif

    /* Open the object where the attribute needs to be checked. */
    ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, 
                                    rtid, cs_scope, &obj_id, &obj_oh);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't open object");

    if(loc_id != obj_id || IOD_OBJ_INVALID == input->loc_attrkv_id) {
        /* get scratch pad of the parent */
        ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* if attribute KV does not exist, return error*/
        if(IOD_OBJ_INVALID == sp[1])
            HGOTO_ERROR_FF(FAIL, "Object has no attributes");

        /* open the attribute KV in scratch pad */
        ret = iod_obj_open_read(coh, sp[1], wtid, NULL, &attr_kv_oh.rd_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
        ret = iod_obj_open_write(coh, sp[1], wtid, NULL, &attr_kv_oh.wr_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
    }
    else {
        /* open the attribute KV  */
        ret = iod_obj_open_read(coh, input->loc_attrkv_id, wtid, NULL, &attr_kv_oh.rd_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
        ret = iod_obj_open_write(coh, input->loc_attrkv_id, wtid, NULL, &attr_kv_oh.wr_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open scratch pad");
    }

    /* close parent group if it is not the location we started the
       traversal into */
    if(loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        ret = iod_obj_close(obj_oh.rd_oh, NULL, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't close object");
    }

    /* get attribute ID */
    ret = H5VL_iod_get_metadata(attr_kv_oh.rd_oh, rtid, H5VL_IOD_LINK, 
                                old_name, cs_scope, NULL, &iod_link);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't retrieve Attribute ID from parent KV store");

    HDassert(iod_link.link_type == H5L_TYPE_HARD);
    attr_id = iod_link.u.iod_id;

    /* remove attribute with old name */
    kv.key = (void *)old_name;
    kv.key_len = 1 + strlen(old_name);
    kvs.kv = &kv;
    kvs.cs = NULL;
    kvs.ret = &ret;
    ret = iod_kv_unlink_keys(attr_kv_oh.wr_oh, wtid, NULL, 1, &kvs, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "Unable to unlink KV pair");

    /* insert attribute with new name */
    ret = H5VL_iod_insert_new_link(attr_kv_oh.wr_oh, wtid, new_name, 
                                   H5L_TYPE_HARD, &attr_id, cs_scope, NULL, NULL);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't insert KV value");

    /* close the Attribute KV object */
    ret = iod_obj_close(attr_kv_oh.rd_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");
    ret = iod_obj_close(attr_kv_oh.wr_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

done:

#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr rename, sending response to client\n");
#endif

    HG_Handler_start_output(op_data->hg_handle, &ret_value);

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_rename_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_rename_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_remove_cb
 *
 * Purpose:	Removes iod HDF5 attribute.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_remove_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                               size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                               size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                               void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_op_in_t *input = (attr_op_in_t *)op_data->input;
    iod_handle_t coh = input->coh; /* container handle */
    iod_handles_t loc_handle = input->loc_oh; /* location handle to start lookup */
    iod_obj_id_t loc_id = input->loc_id; /* The ID of the current location object */
    iod_handles_t obj_oh; /* current object handle accessed */
    iod_handles_t attr_kv_oh; /* KV handle holding attributes for object */
    iod_handle_t attr_oh;
    iod_obj_id_t obj_id, attr_id;
    const char *loc_name = input->path; /* path to start hierarchy traversal */
    const char *attr_name = input->attr_name; /* attribute's name */
    iod_trans_id_t wtid = input->trans_num;
    iod_trans_id_t rtid = input->rcxt_num;
    uint32_t cs_scope = input->cs_scope;
    iod_kv_params_t kvs;
    iod_kv_t kv;
    H5VL_iod_link_t iod_link;
    scratch_pad sp;
    iod_checksum_t sp_cs = 0;
    iod_ret_t ret;
    iod_checksum_t cs;
    int step = 0;
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute Remove %s\n", attr_name);
#endif

    /* Open the object where the attribute needs to be removed. */
    ret = H5VL_iod_server_open_path(coh, loc_id, loc_handle, loc_name, rtid, 
                                    cs_scope, &obj_id, &obj_oh);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't open object");

    if(loc_id != obj_id) {
        /* get scratch pad of the parent */
        ret = iod_obj_get_scratch(obj_oh.rd_oh, rtid, &sp, &sp_cs, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

        if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
            /* verify scratch pad integrity */
            if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
                HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
        }

        /* if attribute KV does not exist, return error*/
        if(IOD_OBJ_INVALID == sp[1])
            HGOTO_ERROR_FF(FAIL, "Object has no attributes");

        /* open the attribute KV in scratch pad */
        ret = iod_obj_open_read(coh, sp[1], wtid, NULL, &attr_kv_oh.rd_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
        ret = iod_obj_open_write(coh, sp[1], wtid, NULL, &attr_kv_oh.wr_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
    }
    else {
        /* open the attribute KV  */
        ret = iod_obj_open_read(coh, input->loc_attrkv_id, wtid, NULL, &attr_kv_oh.rd_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
        ret = iod_obj_open_write(coh, input->loc_attrkv_id, wtid, NULL, &attr_kv_oh.wr_oh, NULL);
        if(ret < 0)
            HGOTO_ERROR_FF(ret, "can't open iod object");
    }

    step ++;

    /* get attribute ID */
    ret = H5VL_iod_get_metadata(attr_kv_oh.rd_oh, rtid, H5VL_IOD_LINK, 
                                attr_name, cs_scope, NULL, &iod_link);
    if(SUCCEED != ret)
        HGOTO_ERROR_FF(ret, "can't retrieve Attribute ID from parent KV store");

    HDassert(iod_link.link_type == H5L_TYPE_HARD);
    attr_id = iod_link.u.iod_id;

    /* remove metadata KV of attribute */
    /* open the attribute */
    ret = iod_obj_open_read(coh, attr_id, wtid, NULL, &attr_oh, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't open current group");

    step ++;

    /* get scratch pad of the attribute */
    ret = iod_obj_get_scratch(attr_oh, rtid, &sp, &sp_cs, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't get scratch pad for object");

    /* close the attribute oh */
    ret = iod_obj_close(attr_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

    step --;

    if(sp_cs && (cs_scope & H5_CHECKSUM_IOD)) {
        /* verify scratch pad integrity */
        if(H5VL_iod_verify_scratch_pad(&sp, sp_cs) < 0)
            HGOTO_ERROR_FF(FAIL, "Scratch Pad failed integrity check");
    }

    ret = iod_obj_unlink(coh, sp[0], wtid, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "Unable to unlink MDKV of attribute object");

    /* remove attribute */
    kv.key = (void *)attr_name;
    kv.key_len = 1 + strlen(attr_name);
    kvs.kv = &kv;
    kvs.cs = NULL;
    kvs.ret = &ret;
    ret = iod_kv_unlink_keys(attr_kv_oh.wr_oh, wtid, NULL, 1, &kvs, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "Unable to unlink KV pair");

    /* close the Attribute KV object */
    ret = iod_obj_close(attr_kv_oh.rd_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");
    ret = iod_obj_close(attr_kv_oh.wr_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

    step --;

    ret = iod_obj_unlink(coh, attr_id, wtid, NULL);
    if(ret != 0)
        HGOTO_ERROR_FF(ret, "Unable to unlink object");

done:
#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr remove, sending response to client\n");
#endif

    /* close parent group if it is not the location we started the
       traversal into */
    if(loc_handle.rd_oh.cookie != obj_oh.rd_oh.cookie) {
        iod_obj_close(obj_oh.rd_oh, NULL, NULL);
    }

    if(step == 2) {
        /* close the attribute oh */
        iod_obj_close(attr_oh, NULL, NULL);
        step --;
    }
    if(step == 1) {
        /* close the Attribute KV object */
        iod_obj_close(attr_kv_oh.rd_oh, NULL, NULL);
        iod_obj_close(attr_kv_oh.wr_oh, NULL, NULL);
        step --;
    }

    HG_Handler_start_output(op_data->hg_handle, &ret_value);

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_op_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);
} /* end H5VL_iod_server_attr_remove_cb() */


/*-------------------------------------------------------------------------
 * Function:	H5VL_iod_server_attr_close_cb
 *
 * Purpose:	Closes iod HDF5 attribute.
 *
 * Return:	Success:	SUCCEED 
 *		Failure:	Negative
 *
 * Programmer:  Mohamad Chaarawi
 *              April, 2013
 *
 *-------------------------------------------------------------------------
 */
void
H5VL_iod_server_attr_close_cb(AXE_engine_t H5_ATTR_UNUSED axe_engine, 
                              size_t H5_ATTR_UNUSED num_n_parents, AXE_task_t H5_ATTR_UNUSED n_parents[], 
                              size_t H5_ATTR_UNUSED num_s_parents, AXE_task_t H5_ATTR_UNUSED s_parents[], 
                              void *_op_data)
{
    op_data_t *op_data = (op_data_t *)_op_data;
    attr_close_in_t *input = (attr_close_in_t *)op_data->input;
    iod_handles_t iod_oh = input->iod_oh; /* iod handle to close */
    iod_ret_t ret;
    //iod_obj_id_t iod_id = input->iod_id; /* iod id of object to close */
    herr_t ret_value = SUCCEED;

#if H5_EFF_DEBUG
    fprintf(stderr, "Start attribute Close %"PRIu64" %"PRIu64"\n",
            iod_oh.rd_oh.cookie, iod_oh.wr_oh.cookie);
#endif

    ret = iod_obj_close(iod_oh.rd_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

    ret = iod_obj_close(iod_oh.wr_oh, NULL, NULL);
    if(ret < 0)
        HGOTO_ERROR_FF(ret, "can't close object");

done:
#if H5_EFF_DEBUG
    fprintf(stderr, "Done with attr close, sending response to client\n");
#endif

    HG_Handler_start_output(op_data->hg_handle, &ret_value);

    HG_Handler_free_input(op_data->hg_handle, input);
    HG_Handler_free(op_data->hg_handle);
    input = (attr_close_in_t *)H5MM_xfree(input);
    op_data = (op_data_t *)H5MM_xfree(op_data);

} /* end H5VL_iod_server_attr_close_cb() */

#endif /* H5_HAVE_EFF */