## how to run the dnn_objDetect app

- cmd format

` ./dnn_objDetect --dnnType [rknn | trt] --pluginPath /path/to/plugin/lib.so --labelTextPath /path/to/label/file.txt --modelPath /path/to/dnn/model/file --imagePath /path/to/image.jpg `

` ./dnn_objDetect --cfg config.ini --imagePath /path/to/image.jpg `

- example cmd

` ./dnn_objDetect --dnnType rknn --pluginPath install/lib/librknn_yolov5.so --labelTextPath ./coco_80_labels_list.txt --modelPath model/RK3588/yolov5s-640-640.rknn --imagePath ./bus.jpg `


` ./dnn_objDetect --dnn rknn --plugin install/lib/librknn_yolov5.so --label ./coco_80_labels_list.txt --model model/RK3588/yolov5s-640-640.rknn --image ./bus.jpg `

` ./dnn_objDetect --dnn rknn --cfg ../perf_cases/dnn_objDetect/dnn.ini --image ./bus.jpg `