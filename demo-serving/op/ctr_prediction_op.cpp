// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "demo-serving/op/ctr_prediction_op.h"
#include <algorithm>
#include <string>
#include "predictor/framework/infer.h"
#include "predictor/framework/memory.h"

namespace baidu {
namespace paddle_serving {
namespace serving {

using baidu::paddle_serving::predictor::MempoolWrapper;
using baidu::paddle_serving::predictor::ctr_prediction::CTRResInstance;
using baidu::paddle_serving::predictor::ctr_prediction::Response;
using baidu::paddle_serving::predictor::ctr_prediction::CTRReqInstance;
using baidu::paddle_serving::predictor::ctr_prediction::Request;

// Total 26 sparse input + 1 dense input
const int CTR_PREDICTION_INPUT_SLOTS = 27;

// First 26: sparse input
const int CTR_PREDICTION_SPARSE_SLOTS = 26;

// Last 1: dense input
const int CTR_PREDICTION_DENSE_SLOT_ID = 26;
const int CTR_PREDICTION_DENSE_DIM = 13;
const int CTR_PREDICTION_EMBEDDING_SIZE = 10;

#if 1
struct CubeValue {
  int error;
  std::string buff;
};

#endif
void fill_response_with_message(Response *response,
                                int err_code,
                                std::string err_msg) {
  if (response == NULL) {
    LOG(ERROR) << "response is NULL";
    return;
  }

  response->set_err_code(err_code);
  response->set_err_msg(err_msg);
  return;
}

int CTRPredictionOp::inference() {
  const Request *req = dynamic_cast<const Request *>(get_request_message());

  TensorVector *in = butil::get_object<TensorVector>();
  Response *res = mutable_data<Response>();

  uint32_t sample_size = req->instances_size();
  if (sample_size <= 0) {
    LOG(WARNING) << "No instances need to inference!";
    fill_response_with_message(res, -1, "Sample size invalid");
    return -1;
  }

  paddle::PaddleTensor lod_tensors[CTR_PREDICTION_INPUT_SLOTS];
  for (int i = 0; i < CTR_PREDICTION_SPARSE_SLOTS; ++i) {
    lod_tensors[i].dtype = paddle::PaddleDType::FLOAT32;
    std::vector<std::vector<size_t>> &lod = lod_tensors[i].lod;
    lod.resize(1);
    lod[0].push_back(0);
  }

  // Query cube API for sparse embeddings
  std::vector<int64_t> keys;
  std::vector<CubeValue> values;

  for (uint32_t si = 0; si < sample_size; ++si) {
    const CTRReqInstance &req_instance = req->instances(si);
    if (req_instance.sparse_ids_size() != CTR_PREDICTION_DENSE_DIM) {
      std::ostringstream iss;
      iss << "dense input size != " << CTR_PREDICTION_DENSE_DIM;
      fill_response_with_message(res, -1, iss.str());
      return -1;
    }

    for (int i = 0; i < req_instance.sparse_ids_size(); ++i) {
      keys.push_back(req_instance.sparse_ids(i));
    }
  }

#if 0
  mCube::CubeAPI* cube = CubeAPI::instance();
  int ret = cube->seek(keys, values);
  if (ret != 0) {
    fill_response_with_message(res, -1, "Query cube for embeddings error");
    LOG(ERROR) << "Query cube for embeddings error";
    return -1;
  }
#else
  float buff[CTR_PREDICTION_EMBEDDING_SIZE] = {
      0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09, 0.00};
  for (int i = 0; i < keys.size(); ++i) {
    values[i].error = 0;
    values[i].buff = std::string(reinterpret_cast<char *>(buff), sizeof(buff));
  }
#endif

  // Sparse embeddings
  for (int i = 0; i < CTR_PREDICTION_SPARSE_SLOTS; ++i) {
    paddle::PaddleTensor lod_tensor = lod_tensors[i];
    std::vector<std::vector<size_t>> &lod = lod_tensor.lod;

    for (uint32_t si = 0; si < sample_size; ++si) {
      const CTRReqInstance &req_instance = req->instances(si);
      lod[0].push_back(lod[0].back() + 1);
    }

    lod_tensor.shape = {lod[0].back(), CTR_PREDICTION_EMBEDDING_SIZE};
    lod_tensor.data.Resize(lod[0].back() * sizeof(float) *
                           CTR_PREDICTION_EMBEDDING_SIZE);

    int offset = 0;
    for (uint32_t si = 0; si < sample_size; ++si) {
      float *data_ptr = static_cast<float *>(lod_tensor.data.data()) + offset;
      const CTRReqInstance &req_instance = req->instances(si);

      int idx = si * CTR_PREDICTION_SPARSE_SLOTS + i;
      if (values[idx].buff.size() !=
          sizeof(float) * CTR_PREDICTION_EMBEDDING_SIZE) {
        LOG(ERROR) << "Embedding vector size not expected";
        fill_response_with_message(
            res, -1, "Embedding vector size not expected");
        return -1;
      }

      memcpy(data_ptr, values[idx].buff.data(), values[idx].buff.size());
      offset += CTR_PREDICTION_EMBEDDING_SIZE;
    }

    in->push_back(lod_tensor);
  }

  // Dense features
  paddle::PaddleTensor lod_tensor = lod_tensors[CTR_PREDICTION_DENSE_SLOT_ID];
  lod_tensor.dtype = paddle::PaddleDType::INT64;
  std::vector<std::vector<size_t>> &lod = lod_tensor.lod;

  for (uint32_t si = 0; si < sample_size; ++si) {
    const CTRReqInstance &req_instance = req->instances(si);
    if (req_instance.dense_ids_size() != CTR_PREDICTION_DENSE_DIM) {
      std::ostringstream iss;
      iss << "dense input size != " << CTR_PREDICTION_DENSE_DIM;
      fill_response_with_message(res, -1, iss.str());
      return -1;
    }
    lod[0].push_back(lod[0].back() + req_instance.dense_ids_size());
  }

  lod_tensor.shape = {lod[0].back(), CTR_PREDICTION_DENSE_DIM};
  lod_tensor.data.Resize(lod[0].back() * sizeof(int64_t));

  int offset = 0;
  for (uint32_t si = 0; si < sample_size; ++si) {
    int64_t *data_ptr = static_cast<int64_t *>(lod_tensor.data.data()) + offset;
    const CTRReqInstance &req_instance = req->instances(si);
    int id_count = req_instance.dense_ids_size();
    memcpy(data_ptr,
           req_instance.dense_ids().data(),
           sizeof(int64_t) * req_instance.dense_ids_size());
    offset += req_instance.dense_ids_size();
  }

  in->push_back(lod_tensor);

  TensorVector *out = butil::get_object<TensorVector>();
  if (!out) {
    LOG(ERROR) << "Failed get tls output object";
    fill_response_with_message(res, -1, "Failed get thread local resource");
    return -1;
  }

  // call paddle fluid model for inferencing
  if (predictor::InferManager::instance().infer(
          CTR_PREDICTION_MODEL_NAME, in, out, sample_size)) {
    LOG(ERROR) << "Failed do infer in fluid model: "
               << CTR_PREDICTION_MODEL_NAME;
    fill_response_with_message(res, -1, "Failed do infer in fluid model");
    return -1;
  }

  if (out->size() != in->size()) {
    LOG(ERROR) << "Output tensor size not equal that of input";
    fill_response_with_message(res, -1, "Output size != input size");
    return -1;
  }

  for (size_t i = 0; i < out->size(); ++i) {
    int dim1 = out->at(i).shape[0];
    int dim2 = out->at(i).shape[1];

    if (out->at(i).dtype != paddle::PaddleDType::FLOAT32) {
      LOG(ERROR) << "Expected data type float";
      fill_response_with_message(res, -1, "Expected data type float");
      return -1;
    }

    float *data = static_cast<float *>(out->at(i).data.data());
    for (int j = 0; j < dim1; ++j) {
      CTRResInstance *res_instance = res->add_predictions();
      res_instance->set_prob0(data[j * dim2]);
      res_instance->set_prob1(data[j * dim2 + 1]);
    }
  }

  for (size_t i = 0; i < in->size(); ++i) {
    (*in)[i].shape.clear();
  }
  in->clear();
  butil::return_object<TensorVector>(in);

  for (size_t i = 0; i < out->size(); ++i) {
    (*out)[i].shape.clear();
  }
  out->clear();
  butil::return_object<TensorVector>(out);

  res->set_err_code(0);
  res->set_err_msg(std::string(""));
  return 0;
}

DEFINE_OP(CTRPredictionOp);

}  // namespace serving
}  // namespace paddle_serving
}  // namespace baidu
