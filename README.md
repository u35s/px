# px

https,http 代理转发工具,支持本地直连和远程转发

本工具是指定某些网站是直接连接还是走代理,例如家庭访问国外的服务器,
会比较慢,如果用阿里云或腾讯云服务器做代理服务器,速度就会快很多,
px能让你自定义规则,以实现国内国外智能上网.

当然,如果你的服务器未能翻墙,可能还需要[h2s](https://github.com/u35s/h2s)来帮你翻墙了

# 安装
```shell
yum install gcc-c++
git clone https://github.com/u35s/px.git && cd px && make 
```
# 使用

```
" 本地直连
./px 8888

" 转发到远程
./px 8888 u35s.com 8888
```

