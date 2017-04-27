#ifndef __RK_VDEC_H__
#define __RK_VDEC_H__

#define MODULE_TAG "mpi_dec"

#include <string.h>
#include "rk_mpi.h"

#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_time.h"

#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "dev.h"
#include "bo.h"
#include "modeset.h"

#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>

#define MPI_DEC_STREAM_SIZE         (SZ_2K)
#define MAX_FILE_NAME_LENGTH        256


typedef void (* Notify)(void);

typedef struct {
    MppCtx          ctx;
    MppApi          *mpi;

    /* end of stream flag when set quit the loop */
    RK_U32          eos;

    /* buffer for stream data reading */
    char            *buf;

    /* input and output */
    MppBufferGroup  frm_grp;
    MppBufferGroup  pkt_grp;
    MppPacket       packet;
    size_t          packet_size;
    MppFrame        frame;

    RK_U32          frame_count;
    RK_U32          size;
} MpiDecLoopData;

typedef struct {
    char            *file_input;
    void            *in_buf;
    void            *out_buf;
    MppCodingType   type;
    RK_U32          width;
    RK_U32          height;
    Notify          notify;

    int dma_fd;
    char *input_file;
    char *output_file;

    /* display parameter */
    int displayed;
    struct sp_dev *dev;
    struct sp_plane **plane;
    struct sp_crtc *test_crtc;
    struct sp_plane *test_plane;

} MpiDecCmd;

MpiDecLoopData *data;
/*static */MpiDecCmd *cmd;

RK_S32 RK_MPI_VDEC_OpenCtx();
RK_S32 RK_MPI_VDEC_Reset();
RK_S32 RK_MPI_VDEC_Deinit();
RK_S32 RK_MPI_VDEC_Init(RK_U32 width, RK_U32 height, RK_U32 type, void *in_buf, void *out_buf, Notify notify);
void init_drm_context(MpiDecCmd *cmd_ctx);
int display_one_frame(MpiDecCmd *cmd_ctx);



#endif // rk_vdec.h
