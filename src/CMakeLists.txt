#该文件夹内的所有源文件
aux_source_directory(. SRC_LIST)
# 编译成库文件
add_library(mprpc ${SRC_LIST})
# 依赖库
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)

