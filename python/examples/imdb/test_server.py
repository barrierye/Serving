# Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# pylint: disable=doc-string-missing

import os
import sys
from paddle_serving_server import OpMaker
from paddle_serving_server import OpSeqMaker
from paddle_serving_server import Server

op_maker = OpMaker()
read_op = op_maker.create('general_reader')
bow_infer_op = op_maker.create(node_type='general_infer', node_name='bow')
cnn_infer_op = op_maker.create(node_type='general_infer', node_name='cnn')
response_op = op_maker.create('general_response')

op_seq_maker = OpSeqMaker()
op_seq_maker.add_op(read_op)
op_seq_maker.add_op(bow_infer_op, dependent_nodes=[read_op])
op_seq_maker.add_op(cnn_infer_op, dependent_nodes=[read_op])
op_seq_maker.add_op(response_op, dependent_nodes=[bow_infer_op, cnn_infer_op])

server = Server()
server.set_op_sequence(op_seq_maker.get_op_sequence())
model_configs = {'bow': './imdb_bow_model', 'cnn': './imdb_cnn_model'}
server.load_model_config(model_configs)
server.prepare_server(workdir="work_dir1", port=8000, device="cpu")
server.run_server()
