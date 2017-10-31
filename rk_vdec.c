/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rk_vdec.h"

#define USE_MEMCPY_TEST 0

void dump_mpp_frame_to_buf(MppFrame frame)
{
    mpp_log("dump_mpp_frame_to_file in\n");
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    void *tmp = NULL;
    tmp = cmd->out_buf;
    int size = 0;

    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;
    if (NULL == frame)
        return;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);
    mpp_log("dump_mpp_frame_to_file width:%d,height:%d,h_stride:%d,v_stride:%d\n", width, height, h_stride, v_stride);

    if (NULL == buffer)
        return;

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);
    cmd->dma_fd = mpp_buffer_get_fd(buffer);
    switch (fmt) {
    case MPP_FMT_YUV420SP : {
#if USE_MEMCPY_TEST
        mpp_log("formart is MPP_FMT_YUV420SP record it\n");
        RK_U32 i;
        RK_U8 *base_y = base;
        RK_U8 *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            memcpy(tmp, base_y, width);
            tmp += width;
            size += width;
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride) {
            memcpy(tmp, base_c, width);
            tmp += width;
            size += width;
        }

        cmd->notify();
#else
		display_one_frame(cmd);
#endif
    } break;
    default : {
        mpp_err("not supported format %d\n", fmt);
    } break;
    }
}

void RK_MPI_VDEC_GET_Frame()
{
    mpp_log("[%s:%d]\n", __func__, __LINE__);
    RK_U32 pkt_done = 0;
    RK_U32 pkt_eos  = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    char   *buf = data->buf;
    MppPacket packet = data->packet;
    MppFrame  frame  = NULL;

    if (data->size != data->packet_size) {
        mpp_log("found last packet\n");

        // setup eos flag
        data->eos = pkt_eos = 1;
    }

    // write data to packet
    mpp_packet_write(packet, 0, buf, data->size);
    // reset pos
    mpp_packet_set_pos(packet, buf);
    mpp_packet_set_length(packet, data->size);

    if (pkt_eos) {
        mpp_packet_set_eos(packet);
    }
    do {
        // send the packet first if packet is not done
        if (!pkt_done) {
            ret = mpi->decode_put_packet(ctx, packet);
            if (MPP_OK == ret)
                pkt_done = 1;
        }
        mpp_log("++=---------------pkt_done %d\n", pkt_done);
        // then get all available frame and release
        do {
            mpp_log("decode_get_frame \n");
            RK_S32 get_frm = 0;
            RK_U32 frm_eos = 0;

            ret = mpi->decode_get_frame(ctx, &frame);
            if (MPP_OK != ret) {
                mpp_err("decode_get_frame failed ret %d\n", ret);
                break;
            }

            if (frame) {
                if (mpp_frame_get_info_change(frame)) {
                    mpp_log("decode_get_frame get info changed found\n");
                    mpi->control(ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                } else {
                    mpp_log("decode_get_frame get frame %d\n", data->frame_count++);
                    dump_mpp_frame_to_buf(frame);
                }
                frm_eos = mpp_frame_get_eos(frame);
                mpp_frame_deinit(&frame);
                frame = NULL;
                get_frm = 1;
            }

            // if last packet is send but last frame is not found continue
            if (pkt_eos && pkt_done && !frm_eos) {
                msleep(10);
                continue;
            }
            if (frm_eos) {
                break;
            }
            if (get_frm)
                continue;

            break;
        } while (1);

        if (pkt_done)
            break;

        msleep(50);
    } while (1);
}

RK_S32 RK_MPI_VDEC_OpenCtx()
{
    mpp_log("[%s:%d]\n", __func__, __LINE__);
    MPP_RET ret         = MPP_OK;
    // base flow context
    MppCtx ctx          = NULL;
    MppApi *mpi         = NULL;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MpiCmd mpi_cmd      = MPP_CMD_BASE;
    MppParam param      = NULL;
    RK_U32 need_split   = 1;

    // paramter for resource malloc
    RK_U32 width        = cmd->width;
    RK_U32 height       = cmd->height;
    MppCodingType type  = cmd->type;

    // resources
    char *buf           = NULL;
    size_t packet_size  = MPI_DEC_STREAM_SIZE;
    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;
    MppBufferGroup frm_grp  = NULL;
    MppBufferGroup pkt_grp  = NULL;

    //MpiDecLoopData data;
    data = malloc(sizeof(MpiDecLoopData));

    mpp_log("mpi_dec_test start\n");
    memset(data, 0, sizeof(MpiDecLoopData));

    buf = mpp_malloc(char, packet_size);
    if (NULL == buf) {
        mpp_err("mpi_dec_test malloc input stream buffer failed\n");
        RK_MPI_VDEC_Deinit();
    }

    ret = mpp_packet_init(&packet, buf, packet_size);
    if (ret) {
        mpp_err("mpp_packet_init failed\n");
        RK_MPI_VDEC_Deinit();
    }

    mpp_log("mpi_dec_test decoder test start w %d h %d type %d\n", width, height, type);

    // decoder demo
    ret = mpp_create(&ctx, &mpi);

    if (MPP_OK != ret) {
        mpp_err("mpp_create failed\n");
        RK_MPI_VDEC_Deinit();
    }

    // NOTE: decoder split mode need to be set before init
    mpi_cmd = MPP_DEC_SET_PARSER_SPLIT_MODE;
    param = &need_split;
    ret = mpi->control(ctx, mpi_cmd, param);
    if (MPP_OK != ret) {
        mpp_err("mpi->control failed\n");
        RK_MPI_VDEC_Deinit();
    }

    ret = mpp_init(ctx, MPP_CTX_DEC, type);
    if (MPP_OK != ret) {
        mpp_err("mpp_init failed\n");
        RK_MPI_VDEC_Deinit();
    }

    data->ctx            = ctx;
    data->mpi            = mpi;
    data->eos            = 0;
    data->buf            = buf;
    data->packet         = packet;
    data->packet_size    = packet_size;
    data->frame          = frame;
    data->frame_count    = 0;

    return ret;

}


RK_S32 RK_MPI_VDEC_Reset()
{
    RK_S32 ret;

    mpp_log("RK_MPI_VDEC_Reset\n");
    ret = data->mpi->reset(data->ctx);
    if (MPP_OK != ret) {
        mpp_err("mpi->reset failed\n");
        RK_MPI_VDEC_Deinit();
    }

    return ret;
}

RK_S32 RK_MPI_VDEC_Deinit()
{
    mpp_log("RK_MPI_VDEC_Deinit in\n");
    if (data->packet) {
        mpp_packet_deinit(&data->packet);
        data->packet = NULL;
    }

    if (data->frame) {
        mpp_frame_deinit(&data->frame);
        data->frame = NULL;
    }

    if (data->ctx) {
        mpp_destroy(data->ctx);
        data->ctx = NULL;
    }

    if (data->buf) {
        mpp_free(data->buf);
        data->buf = NULL;
    }

    if (data) {
        free(data);
        data = NULL;
    }

    if (cmd) {
        free(cmd);
        cmd = NULL;
    }

    return MPP_OK;
}

RK_S32 RK_MPI_VDEC_Init(RK_U32 width, RK_U32 height, RK_U32 type, void *in_buf, void *out_buf, Notify notify)
{
    mpp_log("RK_MPI_VDEC_Init \n");
    RK_S32 ret = MPP_OK;

    if (!cmd) {
        cmd = malloc(sizeof(MpiDecCmd));
        memset((void*)cmd, 0, sizeof(*cmd));
    }
    cmd->type = type;
    cmd->width = width;
    cmd->height = height;
    cmd->in_buf = in_buf;
    cmd->out_buf = out_buf;
    cmd->notify = notify;

    return ret;
}

void init_drm_context(MpiDecCmd *cmd_ctx)
{
    int ret, i;
    cmd_ctx->dev = create_sp_dev();
    if (!cmd_ctx->dev) {
        printf("create_sp_dev failed\n");
        exit(-1);
    }

    ret = initialize_screens(cmd_ctx->dev, cmd_ctx->width, cmd_ctx->height);
    if (ret) {
        printf("initialize_screens failed\n");
        exit(-1);
    }
    cmd_ctx->plane = calloc(cmd_ctx->dev->num_planes, sizeof(*cmd_ctx->plane));
    if (!cmd_ctx->plane) {
        printf("calloc plane array failed\n");
        exit(-1);;
    }

    cmd_ctx->test_crtc = &cmd_ctx->dev->crtcs[0];
    for (i = 0; i < cmd_ctx->test_crtc->num_planes; i++) {
        cmd_ctx->plane[i] = get_sp_plane(cmd_ctx->dev, cmd_ctx->test_crtc);
        if (is_supported_format(cmd_ctx->plane[i], DRM_FORMAT_NV12))
            cmd_ctx->test_plane = cmd_ctx->plane[i];
    }
    if (!cmd_ctx->test_plane) {
        printf("test_plane is NULL\n");
        exit(-1);
    }
}
 void deinit_drm_context(MpiDecCmd *cmd_ctx)
{
	destroy_sp_dev(cmd_ctx->dev);
	if (cmd_ctx->plane) {
		free(cmd_ctx->plane);
		cmd_ctx->plane = NULL;
	}
}

int display_one_frame(MpiDecCmd *cmd_ctx)
{
    int ret;
    struct drm_mode_create_dumb cd;
    struct sp_bo *bo;
    uint32_t handles[4] = {0}, pitches[4]= {0}, offsets[4]= {0};

    bo = calloc(1, sizeof(*bo));
    if (!bo) {
        printf("calloc sp_bo failed\n");
        exit(-1);
    }
#if USE_MEMCPY_TEST
    memcpy(cmd_ctx->test_crtc->scanout->map_addr, cmd->out_buf, cmd_ctx->width * cmd_ctx->height * 3 / 2);
    ret = drmModeSetPlane(cmd_ctx->dev->fd, cmd_ctx->test_plane->plane->plane_id,
                          cmd_ctx->test_crtc->crtc->crtc_id, cmd_ctx->test_crtc->scanout->fb_id, 0, 0, 0,
                          cmd_ctx->test_crtc->crtc->mode.hdisplay,
                          cmd_ctx->test_crtc->crtc->mode.vdisplay,
                          0, 0, cmd_ctx->width << 16, cmd_ctx->height << 16);

#else
    ret = drmPrimeFDToHandle(cmd_ctx->dev->fd, cmd_ctx->dma_fd, &bo->handle);
    bo->dev = cmd_ctx->dev;
    bo->width = (cmd_ctx->width + 15) & (~15);
    bo->height = (cmd_ctx->height+ 15) & (~15);
    bo->depth = 16;
    bo->bpp = 32;
    bo->format = DRM_FORMAT_NV12;
    bo->flags = 0;

    handles[0] = bo->handle;
    pitches[0] = bo->width;
    offsets[0] = 0;
    handles[1] = bo->handle;
    pitches[1] = bo->width;
    offsets[1] = bo->width * bo->height;

    ret = drmModeAddFB2(bo->dev->fd, bo->width, bo->height,
                        bo->format, handles, pitches, offsets,
                        &bo->fb_id, bo->flags);
    if (ret) {
        printf("failed to create fb ret=%d\n", ret);
        exit(-1);
    }

    ret = drmModeSetPlane(cmd_ctx->dev->fd, cmd_ctx->test_plane->plane->plane_id,
                          cmd_ctx->test_crtc->crtc->crtc_id, bo->fb_id, 0, 0, 0,
                          cmd_ctx->test_crtc->crtc->mode.hdisplay,
                          cmd_ctx->test_crtc->crtc->mode.vdisplay,
                          0, 0, cmd_ctx->width << 16, cmd_ctx->height << 16);
#endif

    if (ret) {
        printf("failed to set plane to crtc ret=%d\n", ret);
        exit(-1);

    }


    if (cmd_ctx->test_plane->bo) {
        if (cmd_ctx->test_plane->bo->fb_id) {
            ret = drmModeRmFB(cmd_ctx->dev->fd, cmd_ctx->test_plane->bo->fb_id);
            if (ret)
                printf("Failed to rmfb ret=%d!\n", ret);
        }
        if (cmd_ctx->test_plane->bo->handle) {
            struct drm_gem_close req = {
                .handle = cmd_ctx->test_plane->bo->handle,
            };

            drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
        }
        free(cmd_ctx->test_plane->bo);
    }

    cmd_ctx->test_plane->bo = bo;

    return ret;

}

