#include <stdio.h>
#include <stdlib.h>

#include "rk_vdec.h"
FILE *fd;
void *out_buf;
extern MpiDecLoopData *data;
extern void RK_MPI_VDEC_GET_Frame();

void notity()
{
    mpp_log("notity\n");
    if (cmd->output_file)
        fwrite(cmd->out_buf, 1, cmd->width * cmd->height * 3 / 2, fd);
    if (cmd->displayed)
        display_one_frame(cmd);
}

void parse_options(int argc, char *argv[], MpiDecCmd *cmd_ctx)
{
    int opt;

    while ((opt = getopt(argc, argv, "i:o:d:w:h:")) != -1) {
        switch (opt) {
        case 'i':
            cmd_ctx->input_file = optarg;
            break;
        case 'o':
            cmd_ctx->output_file = optarg;
            break;
        case 'd':
            cmd_ctx->displayed = atoi(optarg);
        case 'w':
            cmd_ctx->width = atoi(optarg);
            break;
        case 'h':
            cmd_ctx->height = atoi(optarg);
            break;
        }
    }

    printf("demo parameters setting:\n");
    printf("	input filename: %s\n", cmd_ctx->input_file);
    printf("	display: %d\n", cmd_ctx->displayed);
    printf("	save output frame filename: %s\n", cmd_ctx->output_file);
    printf("	video width: %d\n", cmd_ctx->width);
    printf("	video hight: %d\n", cmd_ctx->height);
}

int main(int argc, char **argv)
{
    RK_S32 ret = 0;

    cmd = malloc(sizeof(MpiDecCmd));
    memset((void*)cmd, 0, sizeof(*cmd));

    parse_options(argc, argv, cmd);
    if (cmd->width * cmd->height == 0) {
        mpp_err("test failed w*h = %d\n", cmd->width * cmd->height);
        return 0;
    }
    cmd->in_buf = malloc(SZ_64K);
    cmd->out_buf = malloc(cmd->width * cmd->height * 3 / 2);

    RK_MPI_VDEC_Init(cmd->width, cmd->height, MPP_VIDEO_CodingAVC, cmd->in_buf, cmd->out_buf, notity);

    init_drm_context(cmd);
    ret = RK_MPI_VDEC_OpenCtx();

    //FILE *in_f = fopen("/mnt/udisk/sintel.h264", "rb");
    FILE *in_f = fopen(cmd->input_file, "rb");

    fd = fopen(cmd->output_file, "w+b");
    //fd = fopen("/mnt/udisk/frame_lby.yuv", "w+b");

    if (NULL == in_f) {
        mpp_err("failed to open input file \n");
        RK_MPI_VDEC_Deinit();
    }

    mpp_log("[%s:%d]\n", __func__, __LINE__);
    {
        while (!feof(in_f)) {
            data->size = fread(cmd->in_buf, 1, SZ_2K, in_f);
            memcpy(data->buf, cmd->in_buf, SZ_2K);
            RK_MPI_VDEC_GET_Frame();
        }
    }

    RK_MPI_VDEC_Reset();
    RK_MPI_VDEC_Deinit();
    if (cmd) {
        free(cmd);
        cmd = NULL;
    }
    fclose(in_f);

    if (MPP_OK == ret)
        mpp_log("test success\n");
    else
        mpp_err("test failed ret %d\n", ret);

    return 0;
}
