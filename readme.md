## TodoList

- [x] 1. 登录注册功能，不能重复登录，重复注册。用户信息也存储在数据库中。
- [x] 2. 单词查询功能
- [x] 3. 历史记录功能，存储单词，意思，以及查询时间，存储在数据库
- [x] 4. 基于TCP，支持多客户端连接
- [x] 5. 采用数据库保存用户信息与历史记录
- [x] 6. 将dict.txt的数据导入到数据库中保存。
- [x] 7. 返回上级、按下ctrl+c退出客户端后，该客户端退出登录

## 数据包

格式为 `%c\0%s\0%s` 或者 `%c\0%s\0%s`

```txt
功能    编号    \0      数据        \0      数据
注册    s               username           password
登录    l               username           password
登出    o               username
错误    e               message
成功    g               message
查询    q               message
```

