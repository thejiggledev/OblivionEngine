#include <DebugOutput.h>
#include <GraphicsAPI_Vulkan.h>
#include <OpenXRDebugUtils.h>
#include <type_traits>

class oblivion{
  public:
    oblivion(GraphicsAPI_Type apiType)
      : m_apiType(apiType){
        if (!CheckGraphicsAPI_TypeIsValidForPlatform(m_apiType)){
          std::cout << "ERROR: the provided graphics api is not valid for this platform" << std::endl;
          DEBUG_BREAK;
        }
    }
    ~oblivion() = default;
    void Run(){
      CreateInstance();
      CreateDebugMessenger();
      GetInstanceProperties();
      GetSystemID();
      CreateSession();

      while(m_applicationRunning){
        PollSystemEvents();
        PollEvents();
        if (m_sessionRunning) {
          //Probably draw Frame.
        }
      }
      DestroySession();
      DestroyDebugMessenger();
      DestroyInstance();

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
    void CreateInstance(){
      XrApplicationInfo AI;
      strncpy(AI.applicationName, "Oblivion", XR_MAX_APPLICATION_NAME_SIZE);
      AI.applicationVersion = 1;
      strncpy(AI.engineName, "Oblivion Engine", XR_MAX_ENGINE_NAME_SIZE);
      AI.engineVersion = 1;
      AI.apiVersion = XR_CURRENT_API_VERSION;

      m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
      m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_apiType));
      //Get the api layers from the openxr runtime
      
      uint32_t apiLayerCount = 0;
      std::vector<XrApiLayerProperties> apiLayerProperties;
      OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to Enumerate ApiLayerProperties");
      apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
      OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate APILayerProperties");

      //check the requested layers against the ones from OpenXR. If found add to API Layers.
      for (auto &requestLayer : m_apiLayers) {
        for (auto &layerProperty: apiLayerProperties){
          if(strcmp(requestLayer.c_str(), layerProperty.layerName) != 0){
            continue;
          } else {
            m_activeAPILayers.push_back(requestLayer.c_str());
          }
        }
      }
      //Get all Instance Extensions fromt he OPENXR InstanceProcAddr
      uint32_t extensionCount = 0;
      std::vector<XrExtensionProperties> extensionProperties;
      //2-call idiom: first call to get extensionCount and 2nd call to fill out the structs
      OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr),"Failed to Enumerate InstanceExtensionProperties");
      extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
      OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate Instance extensions properties.");

      //check the required instance extensions against the ones from OpenXR runtime.
      //Add if extension is found, otherwise log error.
      for (auto &requestedInstanceExtension : m_instanceExtensions){
        bool found = false;
        for (auto &extensionProperty: extensionProperties){
          if(strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0){
            continue;
          } else {
            m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
            found = true;
            break;
          }
        }
        if(!found){
          XR_TUT_LOG_ERROR("Failed to find OpenXR Extenstion:" << requestedInstanceExtension);
        }
      }
      XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
      instanceCI.createFlags = 0;
      instanceCI.applicationInfo = AI;
      instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
      instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
      instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
      instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();

      OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");

    }//CreateInstance()

    void DestroyInstance(){
      OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
    }//DestroyInstance()

    void CreateDebugMessenger(){
      //Check that "XR_EXT_debug utils" is in the active Instance
      if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)){
        m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance); // from OpenXrDebugUtils.h
      }
    }//CreateDebugMessenger()

    void DestroyDebugMessenger(){
      //Check that "XR_EXT_debug_utils" is in the activate Instance Extensions
      if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
        DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger); // also from OpenXRDebugUtils.h
      }
    }//DestroyDebugMessenger()

    void GetSystemID(){
      //Get the XrSystemId from the instance provided by XRform factor
      XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
      systemGI.formFactor;
      OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get System ID");

      //get the System's properties for some general information about the hardware and vendor.
      OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to Get System properties");
    }//GetSystemID()
     
    void PollEvents(){
      //Poll OpenXR for a new event.
      XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
      auto XrPollEvents = [&]() -> bool {
        eventData = {XR_TYPE_EVENT_DATA_BUFFER};
        return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
      };

      while (XrPollEvents()) {
        switch (eventData.type) {
        // Log the number of lost events from the runtime.
          case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
            XrEventDataEventsLost *eventsLost = reinterpret_cast<XrEventDataEventsLost *>(&eventData);
            XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
            break;
          }
          //Log that an instance loss is pending and shutdown the application.
          case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            XrEventDataInstanceLossPending *instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending *>(&eventData);
            XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
            m_sessionRunning = false;
            m_applicationRunning = false;
            break;
          }
          //Log that the interaction profile has changed 
          case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
            XrEventDataInteractionProfileChanged *interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged *>(&eventData);
            XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
            if (interactionProfileChanged->session != m_session) {
              XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
              break;
            }
            break;
          }
          //Log that theres a reference space chage pending.
          case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&eventData);
            XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
            if (referenceSpaceChangePending->session != m_session) {
              XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown reason");
              break;
            }
            break;
          }
          case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            XrEventDataSessionStateChanged *sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);
            
            if (sessionStateChanged->session != m_session){
              XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
              break;
            }
            
            if (sessionStateChanged->state == XR_SESSION_STATE_READY){
              //Session State is ready, and begin XrSession using the XrviwConfigurationType
              XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
              sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
              OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin XrSession");
              m_sessionRunning = true;
            }

            if(sessionStateChanged->state == XR_SESSION_STATE_STOPPING){
              //SesssionState has stopped, so close XRSession.
              OPENXR_CHECK(xrEndSession(m_session), "Failed to end session");
              m_sessionRunning = false;
            }

            if(sessionStateChanged->state == XR_SESSION_STATE_EXITING){
              //session state is exiting so exit XrSession
              m_sessionRunning = false;
              m_applicationRunning = false;
            }

            if(sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING){
              //session state is lost, exiting application. however u can try to resetablish instance and session here.
              m_sessionRunning = false;
              m_applicationRunning = false;
            }

            //Storestate for rest of application to use
            m_sessionState = sessionStateChanged->state;
            break;
          }
          default: {
            break;
          }
        }
      }
    }

    void GetInstanceProperties(){
      XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
      OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get Instance Properties");
      XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
        << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
        << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
        << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
    }//GetInstanceProperties()

    void CreateSession(){
      XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
      m_graphicsAPI = std::make_unique<GraphicsAPI_Vulkan>(m_xrInstance, m_systemID);
      sessionCI.next = m_graphicsAPI->GetGraphicsBinding();
      sessionCI.createFlags = 0;
      sessionCI.systemId = m_systemID;
      
      OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, & m_session), "Failed to create Session.");
    }
    void DestroySession(){
      OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.")
    }

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
    }//PollSystemEvents()

  private:
   XrInstance m_xrInstance = {};
   std::vector<const char *> m_activeAPILayers = {};
   std::vector<const char *> m_activeInstanceExtensions = {};
   std::vector<std::string> m_apiLayers = {};
   std::vector<std::string> m_instanceExtensions = {};

   XrDebugUtilsMessengerEXT m_debugUtilsMessenger = {};

   XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
   XrSystemId m_systemID = {};
   XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

   GraphicsAPI_Type m_apiType = UNKNOWN;
   std::unique_ptr<GraphicsAPI> m_graphicsAPI = nullptr;

   XrSession m_session = XR_NULL_HANDLE;
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
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

