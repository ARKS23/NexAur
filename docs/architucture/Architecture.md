# NexAur架构
## 初步架构图
![alt text](./images/architecture.png)

## 启动调用链
1. main函数入口
2. 构建引擎，引擎进行初始化函数
3. 引擎初始化函数调用全局上下文启动各个系统模块
4. 运行引擎 `tick`
5. 结束时销毁模块的顺序通常和初始化的顺序相反

### TickOneFrame
1. logicalTick
  1.  `m_world_manager->tick(delta_time);` 更新当前关卡内的所有对象
  2.  `m_input_system->tick();`  处理输入状态的清理和特定逻辑，真正的输入收集在帧尾的`pollEvents()`
2. calculateFPS: 计算FPS
3. `g_runtime_global_context.m_render_system->swapLogicRenderData();`
  - 数据流向: Logical Memory -> Render Memory
4. rendererTick: 渲染系统根据上一步同步来的数据进行绘制渲染
5. pollEevents: 接收输入，数据流向是Hardware -> Input Buffer
