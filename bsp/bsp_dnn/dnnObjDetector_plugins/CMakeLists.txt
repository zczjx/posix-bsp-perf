option(ENABLE_YOLO "Enable support for YOLO" ON)
option(ENABLE_RESNET18 "Enable support for ResNet18 TrafficCamNet" ON)

if(ENABLE_YOLO)
    # add_subdirectory(yolo)
endif()

if(ENABLE_RESNET18)
    add_subdirectory(resnet18)
endif()