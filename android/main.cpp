#include <DebugOutput.h>
#include <GraphicsAPI_Vulkan.h>
#include <OpenXRDebugUtils.h>

class oblivion{
  public:
    oblivion(GraphicsAPI_Type apiType){

    }
    ~oblivion() = default;
    void Run(){

    }//function Run()

    //Android Specific:
    //stored pointer to the android_app structure from android_main
  public:
    static android_app *androidApp;

    //PollSystemEvents custom structure to handle the returned event:
    struct AndroidAppState{
      ANativeWindow *nativeWindow = nullptr;
      bool resumed = false;
    };
    static AndroidAppState androidAppState;
    
    //process the android app command/event return and update androidAppState
    static void AndroidAppHandleCmd(struct android_app *app, int32_t cmd){
      AndroidAppState *appState = (AndroidAppState *)app->userData;

      switch(cmd){
        case APP_CMD_START:{
          break;
        }case APP_CMD_RESUME:{
          appState->resumed = true;
          break;
        }case APP_CMD_PAUSE:{
          appState->resumed = false;
          break;
        }case APP_CMD_STOP:{
          break;
        }case APP_CMD_DESTROY:{
          appState->nativeWindow = nullptr;
          break;
        }case APP_CMD_INIT_WINDOW:{
          appState->nativeWindow = app->window;
          break;
        }case APP_CMD_TERM_WINDOW:{
          appState->nativeWindow = nullptr;
          break;
        }
      }
    }

  private:
    void PollSystemEvents(){
      //check wehtehr android requested that application should be destroyed
      if(androidApp->destroyRequested != 0){
        m_applicationRunning = false;
        return;
      }
      while(true){
        struct android_poll_source *source = nullptr;
        int events = 0;
        // The timeout depends on whether the application is active.
        const int timeoutMilliseconds = (!androidAppState.resumed && !m_sessionRunning && androidApp->destroyRequested == 0) ? -1 : 0;
        if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events, (void**)&source) >= 0) {
          if (source != nullptr) {
            source->process(androidApp, source);
          }
        } else {
          break;
        }
      }
    }// function PollSystemEvents()
  private:
    bool m_applicationRunning = false;
    bool m_sessionRunning = false;
};//class oblivion

void oblivion_main(GraphicsAPI_Type apiType){
      DebugOutput debugOutput;
      XR_TUT_LOG("oblivion");
      oblivion app(apiType);
      app.Run();
    }//oblivion_main()

android_app *oblivion::androidApp = nullptr;
oblivion::AndroidAppState oblivion::androidAppState = {};

void android_main(struct android_app *app){
  //Allow interaction with the JNI and JVM
  // https://developer.android.com/training/articles/perf-jni#threads

  JNIEnv *env;
  app->activity->vm->AttachCurrentThread(&env, nullptr);
  
  // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_KHR_loader_init
  // Load xrInitializeLoaderKHR() function pointer. On Android, the loader must be initialized with variables from android_app *.
  // Without this, there's is no loader and thus our function calls to OpenXR would fail.
  XrInstance m_xrInstance = XR_NULL_HANDLE;
  PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
  OPENXR_CHECK(xrGetInstanceProcAddr(
          XR_NULL_HANDLE, 
          "xrInitializeLoaderKHR", 
          (PFN_xrVoidFunction *)&xrInitializeLoaderKHR), 
        "Failed to get InstanceProcAddr for xrInitializeLoaderKHR."
        );

  //Set user data and callback for poll system events;
  app->userData = &oblivion::androidAppState;
  app->onAppCmd = oblivion::AndroidAppHandleCmd;
  
  oblivion::androidApp = app;

  oblivion_main(VULKAN);
}

