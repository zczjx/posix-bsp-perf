#include "rknn.hpp"
#include <dlfcn.h>

namespace bsp_dnn
{

rknn::rknn():
    m_logger{std::make_unique<BspLogger>("rknn")}{}

rknn::~rknn()
{

    if (m_params.m_outputs.size() > 0)
    {
        rknn_outputs_release(m_params.m_rknnCtx, m_params.m_io_num.n_output, m_params.m_outputs.data());
    }

    if (m_params.m_rknnCtx)
    {
        rknn_destroy(m_params.m_rknnCtx);
    }
    m_params.m_model_data.reset();
}

std::shared_ptr<unsigned char> rknn::loadModelFile(const std::string& modelPath)
{
    FILE* fp;

    fp = fopen(modelPath.c_str(), "rb");

    if (NULL == fp)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "Open file %s failed.", filename);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    m_params.m_model_size = ftell(fp);
    auto data = loadModelData(fp, 0, m_params.m_model_size);
    fclose(fp);
    return data;
}

std::shared_ptr<unsigned char> rknn::loadModelData(FILE* fp, size_t offset, size_t sz)
{
    int ret;

    if (NULL == fp)
    {
        return nullptr;
    }

    ret = fseek(fp, offset, SEEK_SET);

    if (ret != 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "blob seek failure.");
        return nullptr;
    }

    std::shared_ptr<unsigned char> data(new unsigned char[sz], std::default_delete<unsigned char[]>());
    if (nullptr == data)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "blob malloc failure.");
        return nullptr;
    }

    ret = fread(data.get(), 1, sz, fp);
    return data;

}

void rknn::loadModel(const std::string& modelPath)
{
    if (modelPath.empty())
    {
        throw std::runtime_error("modelPath is empty.");
    }

    m_params.m_model_data = loadModelFile(modelPath);

    if (nullptr == m_params.m_model_data )
    {
        throw std::runtime_error("load model failed.");
    }

    auto ret = rknn_init(&m_params.m_rknnCtx, m_params.m_model_data.get(), m_params.m_model_size, 0, nullptr);

    if (ret < 0)
    {
        throw std::runtime_error("rknn_init failed.");
    }

    ret = rknn_query(m_params.m_rknnCtx, RKNN_QUERY_SDK_VERSION, &m_params.m_version, sizeof(m_params.m_version));

    if (ret < 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "rknn_query RKNN_QUERY_SDK_VERSION failed.");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "sdk version: {} driver version: {}", m_params.m_version.api_version, m_params.m_version.drv_version);

    ret = rknn_query(m_params.m_rknnCtx, RKNN_QUERY_IN_OUT_NUM, &m_params.m_io_num, sizeof(m_params.m_io_num));

    if (ret < 0)
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "rknn_query RKNN_QUERY_IN_OUT_NUM failed.");
    }

    m_logger->printStdoutLog(BspLogger::LogLevel::Info, "model input num: {} output num: {}", m_params.m_io_num.n_input, m_params.m_io_num.n_output);

    m_params.m_input_attrs.resize(m_params.m_io_num.n_input);
    memset(m_params.m_input_attrs.data(), 0, sizeof(rknn_tensor_attr) * m_params.m_input_attrs.size());

    for (int i = 0; i < m_params.m_io_num.n_input; i++)
    {
        m_params.m_input_attrs[i].index = i;
        ret = rknn_query(m_params.m_rknnCtx, RKNN_QUERY_INPUT_ATTR, &m_params.m_input_attrs[i], sizeof(m_params.m_input_attrs[i]));

        if (ret < 0)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "rknn_query RKNN_QUERY_INPUT_ATTR failed.");
        }
    }

    m_params.m_output_attrs.resize(m_params.m_io_num.n_output);
    memset(m_params.m_output_attrs.data(), 0, sizeof(rknn_tensor_attr) * m_params.m_output_attrs.size());

    for(int i = 0; i < m_params.m_io_num.n_output; i++)
    {
        m_params.m_output_attrs[i].index = i;
        ret = rknn_query(m_params.m_rknnCtx, RKNN_QUERY_OUTPUT_ATTR, &m_params.m_output_attrs[i], sizeof(m_params.m_output_attrs[i]));

        if (ret < 0)
        {
            m_logger->printStdoutLog(BspLogger::LogLevel::Error, "rknn_query RKNN_QUERY_OUTPUT_ATTR failed.");
        }
    }

    memset(m_params.m_inputs, 0, sizeof(m_params.m_inputs));
}

int rknn::getInputShape(dnnInputShape& shape)
{
    if (m_params.m_input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        shape.height = m_params.m_input_attrs[0].dims[2];
        shape.width  = m_params.m_input_attrs[0].dims[3];
        shape.channel = m_params.m_input_attrs[0].dims[1];
    }
    else
    {
        shape.height = m_params.m_input_attrs[0].dims[1];
        shape.width  = m_params.m_input_attrs[0].dims[2];
        shape.channel = m_params.m_input_attrs[0].dims[3];
    }
    return 0;
}

int rknn::pushInputData(dnnInput& inputData)
{
    if ((inputData.buf == nullptr) || (inputData.size == 0))
    {
        m_logger->printStdoutLog(BspLogger::LogLevel::Error, "inputData.buf is nullptr.");
        return;
    }

    m_params.m_inputs[0].index = inputData.index;
    m_params.m_inputs[0].type         = m_params.m_input_attrs[0].type;
    m_params.m_inputs[0].size         = inputData.size;
    m_params.m_inputs[0].fmt          = m_params.m_input_attrs[0].fmt;
    m_params.m_inputs[0].pass_through = 0;
    m_params.m_inputs[0].buf          = const_cast<void*>(inputData.buf);
    return rknn_inputs_set(m_params.m_rknnCtx, m_params.m_io_num.n_input, m_params.m_inputs);
}

int rknn::popOutputData(std::vector<dnnOutput>& outputVector)
{
    if (m_params.m_outputs.size() != m_params.m_io_num.n_output)
    {
        m_params.m_outputs.resize(m_params.m_io_num.n_output);
        for (int i = 0; i < m_params.m_io_num.n_output; i++)
        {
            std::memset(&m_params.m_outputs[i], 0, sizeof(rknn_output));
            m_params.m_outputs[i].index = i;
            m_params.m_outputs[i].want_float = 0;
        }
    }

    if (outputVector.size() != m_params.m_io_num.n_output)
    {
        outputVector.resize(m_params.m_io_num.n_output);
    }

    int ret = rknn_outputs_get(m_params.m_rknnCtx, m_params.m_io_num.n_output,
                    m_params.m_outputs.data(), nullptr);

    for (int i = 0; i < m_params.m_io_num.n_output; i++)
    {
        outputVector[i].index = m_params.m_outputs[i].index;
        outputVector[i].buf = m_params.m_outputs[i].buf;
        outputVector[i].size = m_params.m_outputs[i].size;
        outputVector[i].dataType = m_params.m_outputs[i].want_float ? "float32" : "int8";
    }

    return ret;
}

int rknn::runInference()
{
    return rknn_run(m_params.m_rknnCtx, nullptr);
}

} // namespace bsp_dnn