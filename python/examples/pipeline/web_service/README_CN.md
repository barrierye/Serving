# Default Pipeline Web Service

这里以 Uci 服务为例来介绍 DefaultPipelineWebService 的使用。

## 获取模型
```
sh get_data.sh
```

## 启动服务

```
python web_service.py &>log.txt &
```

## 测试
```
curl -X POST -k http://localhost:18080/prediction -d '{"key": ["x"], "value": ["0.0137, -0.1136, 0.2553, -0.0692, 0.0582, -0.0727, -0.1583, -0.0584, 0.6283, 0.4919, 0.1856, 0.0795, -0.0332"]}'
```

## 更多
`web_service.py` 和 `local_pipeline_server.py` 本质是相同的。