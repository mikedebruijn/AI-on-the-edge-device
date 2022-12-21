#include "ClassFlowControll.h"

#include "connect_wlan.h"
#include "read_wlanini.h"

#include "freertos/task.h"

#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <dirent.h>
#ifdef __cplusplus
}
#endif

#include "ClassLogFile.h"
#include "time_sntp.h"
#include "Helper.h"
#include "server_ota.h"
#ifdef ENABLE_MQTT
    #include "interface_mqtt.h"
    #include "server_mqtt.h"
#endif //ENABLE_MQTT

#include "server_help.h"
#include "../../include/defines.h"

static const char* TAG = "CTRL";


std::string ClassFlowControll::doSingleStep(std::string _stepname, std::string _host){
    std::string _classname = "";
    std::string result = "";

    ESP_LOGD(TAG, "Step %s start", _stepname.c_str());

    if ((_stepname.compare("[MakeImage]") == 0) || (_stepname.compare(";[MakeImage]") == 0)){
        _classname = "ClassFlowMakeImage";
    }
    if ((_stepname.compare("[Alignment]") == 0) || (_stepname.compare(";[Alignment]") == 0)){
        _classname = "ClassFlowAlignment";
    }
    if ((_stepname.compare(0, 7, "[Digits") == 0) || (_stepname.compare(0, 8, ";[Digits") == 0)) {
        _classname = "ClassFlowCNNGeneral";
    }
    if ((_stepname.compare("[Analog]") == 0) || (_stepname.compare(";[Analog]") == 0)){
        _classname = "ClassFlowCNNGeneral";
    }
#ifdef ENABLE_MQTT
    if ((_stepname.compare("[MQTT]") == 0) || (_stepname.compare(";[MQTT]") == 0)){
        _classname = "ClassFlowMQTT";
    }
#endif //ENABLE_MQTT

#ifdef ENABLE_INFLUXDB
    if ((_stepname.compare("[InfluxDB]") == 0) || (_stepname.compare(";[InfluxDB]") == 0)){
        _classname = "ClassFlowInfluxDB";
    }
#endif //ENABLE_INFLUXDB

    for (int i = 0; i < FlowControll.size(); ++i)
        if (FlowControll[i]->name().compare(_classname) == 0){
            if (!(FlowControll[i]->name().compare("ClassFlowMakeImage") == 0))      // if it is a MakeImage, the image does not need to be included, this happens automatically with the html query.
                FlowControll[i]->doFlow("");
            result = FlowControll[i]->getHTMLSingleStep(_host);
        }

    ESP_LOGD(TAG, "Step %s end", _stepname.c_str());

    return result;
}


std::string ClassFlowControll::TranslateAktstatus(std::string _input)
{
    if (_input.compare("ClassFlowMakeImage") == 0)
        return ("Take Image");
    if (_input.compare("ClassFlowAlignment") == 0)
        return ("Aligning");
    if (_input.compare("ClassFlowCNNGeneral") == 0)
        return ("Digitalization of ROIs");
#ifdef ENABLE_MQTT
    if (_input.compare("ClassFlowMQTT") == 0)
        return ("Sending MQTT");
#endif //ENABLE_MQTT
#ifdef ENABLE_INFLUXDB
    if (_input.compare("ClassFlowInfluxDB") == 0)
        return ("Sending InfluxDB");
#endif //ENABLE_INFLUXDB
    if (_input.compare("ClassFlowPostProcessing") == 0)
        return ("Post-Processing");
    if (_input.compare("ClassFlowWriteList") == 0)
        return ("Writing List");

    return "Unkown Status";
}


std::vector<HTMLInfo*> ClassFlowControll::GetAllDigital() 
{
    if (flowdigit)
    {
        ESP_LOGD(TAG, "ClassFlowControll::GetAllDigital - flowdigit != NULL");
        return flowdigit->GetHTMLInfo();
    }

    std::vector<HTMLInfo*> empty;
    return empty;
}

std::vector<HTMLInfo*> ClassFlowControll::GetAllAnalog()
{
    if (flowanalog)
        return flowanalog->GetHTMLInfo();

    std::vector<HTMLInfo*> empty;
    return empty;
}

t_CNNType ClassFlowControll::GetTypeDigital()
{
    if (flowdigit)
        return flowdigit->getCNNType();

    return t_CNNType::None;
}

t_CNNType ClassFlowControll::GetTypeAnalog()
{
    if (flowanalog)
        return flowanalog->getCNNType();

    return t_CNNType::None;
}



#ifdef ENABLE_MQTT
string ClassFlowControll::GetMQTTMainTopic()
{
    for (int i = 0; i < FlowControll.size(); ++i)
        if (FlowControll[i]->name().compare("ClassFlowMQTT") == 0)
            return ((ClassFlowMQTT*) (FlowControll[i]))->GetMQTTMainTopic();

    return "";
}

bool ClassFlowControll::StartMQTTService() {
    /* Start the MQTT service */
        for (int i = 0; i < FlowControll.size(); ++i) {
            if (FlowControll[i]->name().compare("ClassFlowMQTT") == 0) {
                return ((ClassFlowMQTT*) (FlowControll[i]))->Start(AutoIntervall);
            }  
        } 
    return false;
}
#endif //ENABLE_MQTT

void ClassFlowControll::SetInitialParameter(void)
{
    AutoStart = false;
    SetupModeActive = false;
    AutoIntervall = 10; // Minutes
    flowdigit = NULL;
    flowanalog = NULL;
    flowpostprocessing = NULL;
    disabled = false;
    aktRunNr = 0;
    aktstatus = "Booting ...";
}

bool ClassFlowControll::isAutoStart(long &_intervall)
{
    _intervall = AutoIntervall * 60 * 1000; // AutoInterval: minutes -> ms
    return AutoStart;
}

ClassFlow* ClassFlowControll::CreateClassFlow(std::string _type)
{
    ClassFlow* cfc = NULL;

    _type = trim(_type);

    if (toUpper(_type).compare("[MAKEIMAGE]") == 0)
    {
        cfc = new ClassFlowMakeImage(&FlowControll);
        flowmakeimage = (ClassFlowMakeImage*) cfc;
    }
    if (toUpper(_type).compare("[ALIGNMENT]") == 0)
    {
        cfc = new ClassFlowAlignment(&FlowControll);
        flowalignment = (ClassFlowAlignment*) cfc;
    }
    if (toUpper(_type).compare("[ANALOG]") == 0)
    {
        cfc = new ClassFlowCNNGeneral(flowalignment);
        flowanalog = (ClassFlowCNNGeneral*) cfc;
    }
    if (toUpper(_type).compare(0, 7, "[DIGITS") == 0)
    {
        cfc = new ClassFlowCNNGeneral(flowalignment);
        flowdigit = (ClassFlowCNNGeneral*) cfc;
    }
#ifdef ENABLE_MQTT
    if (toUpper(_type).compare("[MQTT]") == 0)
        cfc = new ClassFlowMQTT(&FlowControll);
#endif //ENABLE_MQTT
#ifdef ENABLE_INFLUXDB
    if (toUpper(_type).compare("[INFLUXDB]") == 0)
        cfc = new ClassFlowInfluxDB(&FlowControll);
#endif //ENABLE_INFLUXDB        
    if (toUpper(_type).compare("[WRITELIST]") == 0)
        cfc = new ClassFlowWriteList(&FlowControll);

    if (toUpper(_type).compare("[POSTPROCESSING]") == 0)
    {
        cfc = new ClassFlowPostProcessing(&FlowControll, flowanalog, flowdigit); 
        flowpostprocessing = (ClassFlowPostProcessing*) cfc;
    }

    if (cfc)                            // Attached only if it is not [AutoTimer], because this is for FlowControll
        FlowControll.push_back(cfc);

    if (toUpper(_type).compare("[AUTOTIMER]") == 0)
        cfc = this;    

    if (toUpper(_type).compare("[DATALOGGING]") == 0)
        cfc = this;  

    if (toUpper(_type).compare("[DEBUG]") == 0)
        cfc = this;  

    if (toUpper(_type).compare("[SYSTEM]") == 0)
        cfc = this;          

    return cfc;
}

void ClassFlowControll::InitFlow(std::string config)
{
    string line;

    flowpostprocessing = NULL;

    ClassFlow* cfc;
    FILE* pFile;
    config = FormatFileName(config);
    pFile = fopen(config.c_str(), "r");

    line = "";

    char zw[1024];
    if (pFile != NULL)
    {
        fgets(zw, 1024, pFile);
        ESP_LOGD(TAG, "%s", zw);
        line = std::string(zw);
    }

    while ((line.size() > 0) && !(feof(pFile)))
    {
        cfc = CreateClassFlow(line);
        if (cfc)
        {
            ESP_LOGD(TAG, "Start ReadParameter (%s)", line.c_str());
            cfc->ReadParameter(pFile, line);
        }
        else
        {
            line = "";
            if (fgets(zw, 1024, pFile) && !feof(pFile))
                {
                    ESP_LOGD(TAG, "Read: %s", zw);
                    line = std::string(zw);
                }
        }
    }

    fclose(pFile);

}

std::string* ClassFlowControll::getActStatus(){
    return &aktstatus;
}

void ClassFlowControll::doFlowMakeImageOnly(string time){
    std::string zw_time;

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        if (FlowControll[i]->name() == "ClassFlowMakeImage") {
            zw_time = gettimestring("%H:%M:%S");
            std::string flowStatus = TranslateAktstatus(FlowControll[i]->name());
            aktstatus = flowStatus + " (" + zw_time + ")";
#ifdef ENABLE_MQTT
            MQTTPublish(mqttServer_getMainTopic() + "/" + "status", flowStatus, false);
#endif //ENABLE_MQTT

            FlowControll[i]->doFlow(time);
        }
    }
}

bool ClassFlowControll::doFlow(string time)
{
    bool result = true;
    std::string zw_time;
    int repeat = 0;

#ifdef DEBUG_DETAIL_ON 
    LogFile.WriteHeapInfo("ClassFlowControll::doFlow - Start");
#endif

    /* Check if we have a valid date/time and if not restart the NTP client */
    if (! getTimeIsSet()) {
        LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Time not set, restarting NTP Client!");
        restartNtpClient();
    }

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        zw_time = gettimestring("%H:%M:%S");
        std::string flowStatus = TranslateAktstatus(FlowControll[i]->name());
        aktstatus = flowStatus + " (" + zw_time + ")";
#ifdef ENABLE_MQTT
        MQTTPublish(mqttServer_getMainTopic() + "/" + "status", flowStatus, false);
#endif //ENABLE_MQTT

        string zw = "FlowControll.doFlow - " + FlowControll[i]->name();
        #ifdef DEBUG_DETAIL_ON 
            LogFile.WriteHeapInfo(zw);
        #endif

        if (!FlowControll[i]->doFlow(time)){
            repeat++;
            LogFile.WriteToFile(ESP_LOG_WARN, TAG, "Fehler im vorheriger Schritt - wird zum " + to_string(repeat) + ". Mal wiederholt");
            if (i) i -= 1;    // vPrevious step must be repeated (probably take pictures)
            result = false;
            if (repeat > 5) {
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Wiederholung 5x nicht erfolgreich --> reboot");
                doReboot();
                //Step was repeated 5x --> reboot
            }
        }
        else
        {
            result = true;
        }
        
#ifdef DEBUG_DETAIL_ON  
        LogFile.WriteHeapInfo("ClassFlowControll::doFlow");
#endif

    }
    zw_time = gettimestring("%H:%M:%S");
    std::string flowStatus = "Flow finished";
    aktstatus = flowStatus + " (" + zw_time + ")";
#ifdef ENABLE_MQTT
    MQTTPublish(mqttServer_getMainTopic() + "/" + "status", flowStatus, false);
#endif //ENABLE_MQTT
    return result;
}


string ClassFlowControll::getReadoutAll(int _type)
{
    std::string out = "";
    if (flowpostprocessing)
    {
        std::vector<NumberPost*> *numbers = flowpostprocessing->GetNumbers();

        for (int i = 0; i < (*numbers).size(); ++i)
        {
            out = out + (*numbers)[i]->name + "\t";
            switch (_type) {
                case READOUT_TYPE_VALUE:
                    out = out + (*numbers)[i]->ReturnValue;
                    break;
                case READOUT_TYPE_PREVALUE:
                    if (flowpostprocessing->PreValueUse)
                    {
                        if ((*numbers)[i]->PreValueOkay)
                            out = out + (*numbers)[i]->ReturnPreValue;
                        else
                            out = out + "PreValue too old";                
                    }
                    else
                        out = out + "PreValue deactivated";
                    break;
                case READOUT_TYPE_RAWVALUE:
                    out = out + (*numbers)[i]->ReturnRawValue;
                    break;
                case READOUT_TYPE_ERROR:
                    out = out + (*numbers)[i]->ErrorMessageText;
                    break;
            }
            if (i < (*numbers).size()-1)
                out = out + "\r\n";
        }
    //    ESP_LOGD(TAG, "OUT: %s", out.c_str());
    }

    return out;
}	


string ClassFlowControll::getReadout(bool _rawvalue = false, bool _noerror = false)
{
    if (flowpostprocessing)
        return flowpostprocessing->getReadoutParam(_rawvalue, _noerror);

    string zw = "";
    string result = "";

    for (int i = 0; i < FlowControll.size(); ++i)
    {
        zw = FlowControll[i]->getReadout();
        if (zw.length() > 0)
        {
            if (result.length() == 0)
                result = zw;
            else
                result = result + "\t" + zw;
        }
    }

    return result;
}

string ClassFlowControll::GetPrevalue(std::string _number)	
{
    if (flowpostprocessing)
    {
        return flowpostprocessing->GetPreValue(_number);   
    }

    return std::string("");    
}

std::string ClassFlowControll::UpdatePrevalue(std::string _newvalue, std::string _numbers, bool _extern)
{
    float zw;
    char* p;

    _newvalue = trim(_newvalue);
//    ESP_LOGD(TAG, "Input UpdatePreValue: %s", _newvalue.c_str());

    if (_newvalue.compare("0.0") == 0)
    {
        zw = 0;
    }
    else
    {
        zw = strtof(_newvalue.c_str(), &p);
        if (zw == 0)
            return "- Error in String to Value Conversion!!! Must be of format value=123.456";
    }
    

    if (flowpostprocessing)
    {
        flowpostprocessing->SetPreValue(zw, _numbers, _extern);
        return _newvalue;    
    }

    return std::string();
}

bool ClassFlowControll::ReadParameter(FILE* pfile, string& aktparamgraph)
{
    std::vector<string> splitted;

    aktparamgraph = trim(aktparamgraph);

    if (aktparamgraph.size() == 0)
        if (!this->GetNextParagraph(pfile, aktparamgraph))
            return false;


    if ((toUpper(aktparamgraph).compare("[AUTOTIMER]") != 0) && (toUpper(aktparamgraph).compare("[DEBUG]") != 0) &&
        (toUpper(aktparamgraph).compare("[SYSTEM]") != 0 && (toUpper(aktparamgraph).compare("[DATALOGGING]") != 0)))      // Paragraph passt nicht zu MakeImage
        return false;

    while (this->getNextLine(pfile, &aktparamgraph) && !this->isNewParagraph(aktparamgraph))
    {
        splitted = ZerlegeZeile(aktparamgraph, " =");
        if ((toUpper(splitted[0]) == "AUTOSTART") && (splitted.size() > 1))
        {
            if (toUpper(splitted[1]) == "TRUE")
            {
                AutoStart = true;
            }
        }

        if ((toUpper(splitted[0]) == "INTERVALL") && (splitted.size() > 1))
        {
            AutoIntervall = std::stof(splitted[1]);
        }

        if ((toUpper(splitted[0]) == "DATALOGACTIVE") && (splitted.size() > 1))
        {
            if (toUpper(splitted[1]) == "TRUE")
            {
                LogFile.SetDataLogToSD(true);
            }
            else {
                LogFile.SetDataLogToSD(false);
            }
        }

        if ((toUpper(splitted[0]) == "DATALOGRETENTIONINDAYS") && (splitted.size() > 1))
        {
            LogFile.SetDataLogRetention(std::stoi(splitted[1]));
        }

        if ((toUpper(splitted[0]) == "LOGFILE") && (splitted.size() > 1))
        {
            /* matches esp_log_level_t */
            if ((toUpper(splitted[1]) == "TRUE") || (toUpper(splitted[1]) == "2"))
            {
                LogFile.setLogLevel(ESP_LOG_WARN);
            }
            else if ((toUpper(splitted[1]) == "FALSE") || (toUpper(splitted[1]) == "0") || (toUpper(splitted[1]) == "1"))
            {
                LogFile.setLogLevel(ESP_LOG_ERROR);
            }
            else if (toUpper(splitted[1]) == "3")
            {
                LogFile.setLogLevel(ESP_LOG_INFO);
            }
            else if (toUpper(splitted[1]) == "4")
            {
                LogFile.setLogLevel(ESP_LOG_DEBUG);
            }
        }
        if ((toUpper(splitted[0]) == "LOGFILERETENTIONINDAYS") && (splitted.size() > 1))
        {
            LogFile.SetLogFileRetention(std::stoi(splitted[1]));
        }

        if ((toUpper(splitted[0]) == "TIMEZONE") && (splitted.size() > 1))
        {
            string zw = "Set TimeZone: " + splitted[1];
            setTimeZone(splitted[1]);
        }      

        if ((toUpper(splitted[0]) == "TIMESERVER") && (splitted.size() > 1))
        {
            string zw = "Set TimeZone: " + splitted[1];
            reset_servername(splitted[1]);
        }  

        if ((toUpper(splitted[0]) == "HOSTNAME") && (splitted.size() > 1))
        {
            if (ChangeHostName("/sdcard/wlan.ini", splitted[1]))
            {
                // reboot necessary so that the new wlan.ini is also used !!!
                fclose(pfile);
                LogFile.WriteToFile(ESP_LOG_ERROR, TAG, "Rebooting to activate new HOSTNAME...");
                esp_restart();
                hard_restart();                   
                doReboot();
            }
        }

        if ((toUpper(splitted[0]) == "SETUPMODE") && (splitted.size() > 1))
        {
            if (toUpper(splitted[1]) == "TRUE")
            {
                SetupModeActive = true;
            }        
        }
    }
    return true;
}


int ClassFlowControll::CleanTempFolder() {
    const char* folderPath = "/sdcard/img_tmp";
    
    ESP_LOGD(TAG, "Clean up temporary folder to avoid damage of sdcard sectors: %s", folderPath);
    DIR *dir = opendir(folderPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir: %s", folderPath);
        return -1;
    }

    struct dirent *entry;
    int deleted = 0;
    while ((entry = readdir(dir)) != NULL) {
        std::string path = string(folderPath) + "/" + entry->d_name;
		if (entry->d_type == DT_REG) {
			if (unlink(path.c_str()) == 0) {
				deleted ++;
			} else {
				ESP_LOGE(TAG, "can't delete file: %s", path.c_str());
			}
        } else if (entry->d_type == DT_DIR) {
			deleted += removeFolder(path.c_str(), TAG);
		}
    }
    closedir(dir);
    ESP_LOGD(TAG, "%d files deleted", deleted);
    
    return 0;
}


esp_err_t ClassFlowControll::SendRawJPG(httpd_req_t *req)
{
    return flowmakeimage != NULL ? flowmakeimage->SendRawJPG(req) : ESP_FAIL;
}


esp_err_t ClassFlowControll::GetJPGStream(std::string _fn, httpd_req_t *req)
{
    ESP_LOGD(TAG, "ClassFlowControll::GetJPGStream %s", _fn.c_str());

    CImageBasis *_send = NULL;
    esp_err_t result = ESP_FAIL;
    bool Dodelete = false;    

    if (flowalignment == NULL)
    {
        ESP_LOGD(TAG, "Can't continue, flowalignment is NULL");
        return ESP_FAIL;
    }

    if (_fn == "alg.jpg")
    {
        _send = flowalignment->ImageBasis;  
    }
    else
    {
        if (_fn == "alg_roi.jpg")
        {
            CImageBasis* _imgzw = new CImageBasis(flowalignment->ImageBasis);
            flowalignment->DrawRef(_imgzw);
            if (flowdigit) flowdigit->DrawROI(_imgzw);
            if (flowanalog) flowanalog->DrawROI(_imgzw);
            _send = _imgzw;
            Dodelete = true;
        }
        else
        {
            std::vector<HTMLInfo*> htmlinfo;
            htmlinfo = GetAllDigital();
            ESP_LOGD(TAG, "After getClassFlowControll::GetAllDigital");

            for (int i = 0; i < htmlinfo.size(); ++i)
            {
                if (_fn == htmlinfo[i]->filename)
                {
                    if (htmlinfo[i]->image)
                        _send = htmlinfo[i]->image;
                }
                if (_fn == htmlinfo[i]->filename_org)
                {
                    if (htmlinfo[i]->image_org)
                        _send = htmlinfo[i]->image_org;        
                }
                delete htmlinfo[i];
            }
            htmlinfo.clear();

            if (!_send)
            {
                htmlinfo = GetAllAnalog();
                for (int i = 0; i < htmlinfo.size(); ++i)
                {
                    if (_fn == htmlinfo[i]->filename)
                    {
                        if (htmlinfo[i]->image)
                            _send = htmlinfo[i]->image;
                    }
                    if (_fn == htmlinfo[i]->filename_org)
                    {
                        if (htmlinfo[i]->image_org)
                            _send = htmlinfo[i]->image_org;        
                    }
                    delete htmlinfo[i];
                }
                htmlinfo.clear();

            }
        }
    }

    if (_send)
    {
        ESP_LOGD(TAG, "Sending file: %s ...", _fn.c_str());
        set_content_type_from_file(req, _fn.c_str());
        result = _send->SendJPGtoHTTP(req);
        ESP_LOGD(TAG, "File sending complete");    
        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_send_chunk(req, NULL, 0);
    }

    if (Dodelete) 
    {
        delete _send;
    }

    return result;
}


string ClassFlowControll::getNumbersName()
{
    return flowpostprocessing->getNumbersName();
}

string ClassFlowControll::getJSON()
{
    return flowpostprocessing->GetJSON();
}
