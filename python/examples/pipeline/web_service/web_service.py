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
try:
    from paddle_serving_server_gpu.web_service import DefaultPipelineWebService
except ImportError:
    from paddle_serving_server.web_service import DefaultPipelineWebService
import logging
import numpy as np

_LOGGER = logging.getLogger()


class UciService(DefaultPipelineWebService):
    def init_separator(self):
        self.separator = ","

    def preprocess(self, input_dict):
        # _LOGGER.info(input_dict)
        x_value = input_dict["x"]
        if isinstance(x_value, (str, unicode)):
            input_dict["x"] = np.array(
                [float(x.strip()) for x in x_value.split(self.separator)])
        return input_dict

    def postprocess(self, input_dict, fetch_dict):
        # _LOGGER.info(fetch_dict)
        fetch_dict["price"] = str(fetch_dict["price"][0][0])
        return fetch_dict


uci_service = UciService(name="uci")
uci_service.init_separator()
uci_service.load_model_config("./uci_housing_model")
try:
    uci_service.set_gpus("0")
except Exception:
    pass
uci_service.prepare_server(workdir="workdir", port=18080)
uci_service.run_service()