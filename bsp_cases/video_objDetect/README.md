## how to run the video_objDetect app

- cmd format

` ./video_objDetect --dnn [rknn | trt] --plugin /path/to/plugin/lib.so --label /path/to/label/file.txt --model /path/to/dnn/model/file --decoder [rkmpp | ffmpeg] --encoder [rkmpp | ffmpeg] --g2d [rkrga | nvg2d] --video /path/to/video.h264 --output /path/to/out.h264`

` ./video_objDetect --cfg config.ini --video /path/to/video.h264 --output /path/to/out.h264 `

- example cmd

` ./video_objDetect --dnn rknn --decoder rkmpp --encoder rkmpp --g2d rkrga --plugin install/lib/librga_rknn_yolov5.so --label ./coco_80_labels_list.txt --model model/RK3588/yolov5s-640-640.rknn --video /path/to/video.h264 --output /path/to/out.h264`

` ./video_objDetect --cfg ../perf_cases/video_objDetect/video_dnn.ini --video /path/to/video.h264 --output /path/to/out.h264 `