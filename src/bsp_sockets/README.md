# Epoll and Poll performance
use Google perfetto to illustrate the performance of Epoll and Poll


## BSP Trace Event Visualization

upload the *.perfetto profiler file to [perfetto](https://ui.perfetto.dev/) to analysis the perf data at local web browser

the comparison show the first run of processEvents function use 100us in average, while the first run of processEvents function use 140us in average, so the Epol is faster

- BspTrace visualization demo

  ![Perfetto demo](image/perfetto.PNG)
  
- aio_tcpclient_Epoll demo

  ![](image/aio_tcpclient_Epoll.png)
  
- aio_tcpclient_Poll demo

  ![](image/aio_tcpclient_Poll.png)
  
- aio_tcpserver_Epoll demo

  ![](image/aio_tcpserver_Epoll.png)
  
- aio_tcpserver_Poll demo

  ![](image/aio_tcpserver_Poll.png)

