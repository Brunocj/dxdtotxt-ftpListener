#include "DWLoadLib.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define WIN32
//#define WIN64

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

int main(int argc, char *argv[])
{
    int i, j, k, l;
    struct DWFileInfo fi;
    struct DWMeasurementInfo mi;
    __int64 sample_cnt;
    double* data;
    char* binData;
    void* raw_data;
    struct DWCANPortData* CANPortData;
    struct DWComplex* complex_data;
    double* time_stamp;
    int ch_list_cnt;
    int multifile_id;
    struct DWChannel* ch_list;
    int event_list_cnt;
    struct DWEvent* event_list;
    int reduced_data_cnt;
    struct DWReducedValue* reduced_data;
    double block_size;
    char str[256];
    char* text_values;
    struct TBinaryData* bin_recs;
    int array_info_cnt;
    struct DWArrayInfo* array_info_list;
    double ch_scale, ch_offset;
    void* ret_buff;
    int max_len, data_type, file_index, num_readers, xml_len, xml_props_len, data_type_len;
    int* data_types;
    int* data_type_lens;
    enum DWChannelType channel_type;
    char* out_text;
    char* stream_name;
    int success;
    int data_len, data_offset;
    const int MAX_BIN_SAMPLES = 100;
    const int MAX_BIN_LEN = 10000;
    int read_samples;
    char** filenames;

#ifdef _WIN32
    #ifdef _WIN64
        printf("WIN x64 Application\n");
        const char* DLL_PATH = TEXT("DWDataReaderLib64.dll");
    #else
        printf("WIN x86 Application\n");
        const char* DLL_PATH = TEXT("DWDataReaderLib.dll");
    #endif


    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    filenames = malloc(sizeof(char*) * (argc - 1));
    for (i = 0; i < argc - 1; i++)
    {
        int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argvw[i+1], -1, 0, 0, 0, 0);
        if (len <= 0) {
            printf("Cannot parse input argument: %ws", argvw[i+1]);
            return 1;
        }
        filenames[i] = malloc(sizeof(char) * (len + 1));
        filenames[i][len] = 0;
        len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argvw[i+1], -1, filenames[i], len, 0, 0);
        if (len <= 0) {
            printf("Cannot parse input argument: %ws", argvw[i+1]);
            return 1;
        }
    }
#else
    const char DLL_PATH[] = "DWDataReaderLib.so";

    filenames = (char**)malloc(sizeof(char*) * (argc - 1));
    for (i = 0; i < argc - 1; i++)
    {
        int len = strlen(argv[i + 1]);
        filenames[i] = (char*)malloc(sizeof(char) * (len + 1));
        filenames[i][len] = 0;
        memcpy(filenames[i], argv[i + 1], sizeof(char) * len);
    }
#endif

    if (!LoadDWDLL(DLL_PATH))// Load DLL
    {
        printf("Could not load dll object\n");
        return 0;
    }

    DWInit(); //initiate dll
    printf("DW Version = %i\n\n", DWGetVersion()); //Get dll version

    if (argc < 2)
    {
        printf("Program must be called with at least one parameter (each parameter corresponding to a d7d file)\n");
        return 0;
    }

    for (file_index = 0; file_index < argc - 1; file_index++)
    {
        // DWInit() already creates a data reader and adds it to the list
        // to add additional data readers DWAddReader() must be called
        if (file_index > 0)
        {
            DWAddReader();											// add another data reader
            DWGetNumReaders(&num_readers);							// get number of readers in the list
            if ((file_index >= 0) && (file_index < num_readers))
            {
                DWSetActiveReader(file_index);						// set active reader
            }
        }

        if (!DWOpenDataFile(filenames[file_index], &fi)) //open dewesoft file
        {
            printf("****************\n");
            printf("DATA FILE #%i\n", file_index + 1);
            printf("****************\n\n");

            multifile_id = DWGetMultiFileIndex();

            //Export Header
            DWExportHeader("c:\\SetupFile.xml");

            DWGetMeasurementInfo(&mi);

            //Get File Information
            printf("FILE INF: sample rate = %fHz, start measure time = %lf, start store time = %lf days, duration = %fsec, measure index = %d \n", mi.sample_rate, mi.start_measure_time, mi.start_store_time, mi.duration, multifile_id);

            //storing type
            printf("Storing type=%d \n", DWGetStoringType());

            //Get Data Header
            printf("\nDATA HEADER:\n");
            ch_list_cnt = DWGetHeaderEntryCount();//get channels count
            ch_list = (struct DWChannel*)malloc(sizeof(struct DWChannel) * ch_list_cnt);
            DWGetHeaderEntryList(ch_list);// get all channels
            for (i = 0; i < ch_list_cnt; i++)
            {
                DWGetHeaderEntryText(ch_list[i].index, str, 255);
                printf("Name=%s Unit=%s value=%s\n", ch_list[i].name, ch_list[i].unit, str);
            }
            free(ch_list);

            //Get all events (start trigger, stop trigger, ...)
            printf("\nEVENTS:\n");
            event_list_cnt = DWGetEventListCount();//get event list count
            event_list = (struct DWEvent*)malloc(sizeof(struct DWEvent) * event_list_cnt);
            DWGetEventList(event_list);
            for (i = 0; i < event_list_cnt; i++)
                printf("EVENT: type = %i, text = %s, position = %fsec \n", event_list[i].event_type, event_list[i].event_text, event_list[i].time_stamp);
            free(event_list);

            //Get particular stream
            stream_name = "MEASINFO";
            if (DWGetStream(stream_name, NULL, &max_len) == DWSTAT_OK)
            {
                ret_buff = malloc(max_len);
                memset(ret_buff, 0, max_len);
                DWGetStream(stream_name, (char*)ret_buff, (int*)&max_len);
                printf("\n%s=\n%s\n", stream_name, (char*)ret_buff);
                free(ret_buff);
            }
            else
                printf("\n%s stream not found\n", stream_name);

            //Get all channels
            printf("\nCHANNELS:\n");
            ch_list_cnt = DWGetChannelListCount();//get channels count
            ch_list = (struct DWChannel*)malloc(sizeof(struct DWChannel) * ch_list_cnt);

            data_types = (int*)malloc(ch_list_cnt * sizeof(int));
            data_type_lens = (int*)malloc(ch_list_cnt * sizeof(int));

            DWGetChannelList(ch_list);// get all channels
            for (i = 0; i < ch_list_cnt; i++)
            {
                DWGetChannelFactors(ch_list[i].index, &ch_scale, &ch_offset);
                printf("Index=%i Name=%s Unit=%s Description=%s Scale=%f Offset=%f ", ch_list[i].index, ch_list[i].name, ch_list[i].unit, ch_list[i].description, ch_scale, ch_offset);

                // channel type (sync/async/single value)
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_TYPE, ret_buff, &max_len);
                channel_type = *(enum DWChannelType*)ret_buff;
                free(ret_buff);

                switch (channel_type)
                {
                case DW_CH_TYPE_SYNC:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"sync\"") + 1);
                    strcpy(out_text, "\"sync\"");
                    break;
                case DW_CH_TYPE_ASYNC:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"async\"") + 1);
                    strcpy(out_text, "\"async\"");
                    break;
                case DW_CH_TYPE_SV:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"single value\"") + 1);
                    strcpy(out_text, "\"single value\"");
                    break;
                default:
                    break;
                }

                printf("Channel type=%s ", out_text);
                free(out_text);

                // channel data type
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_DATA_TYPE, ret_buff, &max_len);
                data_type = *(int*)ret_buff;
                free(ret_buff);

                switch (data_type)
                {
                case dtByte:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"byte\"") + 1);
                    strcpy(out_text, "\"byte\"");
                    break;
                case dtShortInt:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"short int\"") + 1);
                    strcpy(out_text, "\"short int\"");
                    break;
                case dtSmallInt:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"small int\"") + 1);
                    strcpy(out_text, "\"small int\"");
                    break;
                case dtWord:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"word\"") + 1);
                    strcpy(out_text, "\"word\"");
                    break;
                case dtInteger:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"integer\"") + 1);
                    strcpy(out_text, "\"integer\"");
                    break;
                case dtSingle:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"single\"") + 1);
                    strcpy(out_text, "\"single\"");
                    break;
                case dtInt64:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"int64\"") + 1);
                    strcpy(out_text, "\"int64\"");
                    break;
                case dtDouble:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"double\"") + 1);
                    strcpy(out_text, "\"double\"");
                    break;
                case dtLongword:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"long word\"") + 1);
                    strcpy(out_text, "\"long word\"");
                    break;
                case dtComplexSingle:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"complex single\"") + 1);
                    strcpy(out_text, "\"complex single\"");
                    break;
                case dtComplexDouble:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"complex double\"") + 1);
                    strcpy(out_text, "\"complex double\"");
                    break;
                case dtText:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"text\"") + 1);
                    strcpy(out_text, "\"text\"");
                    break;
                case dtCANPortData:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"CAN port data\"") + 1);
                    strcpy(out_text, "\"CAN port data\"");
                    break;
                case dtCANFDPortData:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"CAN FD port data\"") + 1);
                    strcpy(out_text, "\"CAN FD port data\"");
                    break;
                case dtBinary:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"Binary data\"") + 1);
                    strcpy(out_text, "\"Binary data\"");
                    break;
                case dtBytes8:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"Bytes[8]\"") + 1);
                    strcpy(out_text, "\"Bytes[8]\"");
                    break;
                case dtBytes16:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"Bytes[16]\"") + 1);
                    strcpy(out_text, "\"Bytes[16]\"");
                    break;
                case dtBytes32:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"Bytes[32]\"") + 1);
                    strcpy(out_text, "\"Bytes[32]\"");
                    break;
                case dtBytes64:
                    out_text = (char*)malloc(sizeof(char) * strlen("\"Bytes[64]\"") + 1);
                    strcpy(out_text, "\"Bytes[64]\"");
                    break;
                }

                data_types[i] = data_type;
                printf("Data type=%s ", out_text);
                free(out_text);

                // channel data type length in bytes
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_DATA_TYPE_LEN_BYTES, ret_buff, &max_len);
                data_type_len = *(int*)ret_buff;
                data_type_lens[i] = data_type_len;
                free(ret_buff);

                // get channel index length
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_INDEX_LEN, ret_buff, &max_len);
                max_len = *(int*)ret_buff;
                free(ret_buff);

                // get channel index
                ret_buff = malloc(max_len);
                memset(ret_buff, 0, max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_INDEX, ret_buff, &max_len);
                printf("DWIndex=\"%s\"\n", (char*)ret_buff);
                free(ret_buff);

                // channel scale & offset (alternative to DWGetChannelFactors method)
                max_len = sizeof(double);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_SCALE, ret_buff, &max_len);
                ch_scale = *(double*)ret_buff;
                DWGetChannelProps(ch_list[i].index, DW_CH_OFFSET, ret_buff, &max_len);
                ch_offset = *(double*)ret_buff;
                free(ret_buff);

                // get length of XML
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_XML_LEN, ret_buff, &max_len);
                xml_len = *(int*)ret_buff;
                free(ret_buff);

                // channel's XML node
                ret_buff = malloc(xml_len);
                memset(ret_buff, 0, xml_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_XML, ret_buff, &xml_len);
                printf("\nXML node=\n%s\n", (char*)ret_buff);
                free(ret_buff);

                // get channel long name length
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_LONGNAME_LEN, ret_buff, &max_len);
                max_len = *(int*)ret_buff;
                free(ret_buff);

                // get channel long name
                ret_buff = malloc(max_len);
                memset(ret_buff, 0, max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_LONGNAME, ret_buff, &max_len);
                printf("\nLong Name=\"%s\"\n", (char*)ret_buff);
                free(ret_buff);

                // get length of XML properties
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_XMLPROPS_LEN, ret_buff, &max_len);
                xml_props_len = *(int*)ret_buff;
                free(ret_buff);

                // channel's XML properties
                if (xml_props_len > 0)
                {
                    ret_buff = malloc(xml_props_len);
                    memset(ret_buff, 0, xml_props_len);
                    DWGetChannelProps(ch_list[i].index, DW_CH_XMLPROPS, ret_buff, &xml_props_len);
                    printf("\nXML properties=\n%s\n\n", (char*)ret_buff);
                    free(ret_buff);
                }

                // get custom properties count
                max_len = sizeof(int);
                ret_buff = malloc(max_len);
                DWGetChannelProps(ch_list[i].index, DW_CH_CUSTOMPROPS_COUNT, ret_buff, &max_len);
                int customPropertiesCount = *(int*)ret_buff;
                free(ret_buff);

                // channel's custom properties
                if (customPropertiesCount > 0)
                {
                    printf("\n Custom properties count =%d\n", customPropertiesCount);

                    int customPropertiesLength = customPropertiesCount * sizeof(struct DWCustomProp);

                    ret_buff = malloc(customPropertiesLength);
                    DWGetChannelProps(ch_list[i].index, DW_CH_CUSTOMPROPS, ret_buff, &customPropertiesLength);
                    printf("\nXML custom properties=\n%s\n\n", (char*)ret_buff);
                    struct DWCustomProp* typed_ret_buff = (struct DWCustomProp*) ret_buff;
                    for (i = 0; i < customPropertiesCount; i++)
                    {
                        printf("XML custom properties key=\"%s\" ", typed_ret_buff[i].key);
                        switch (typed_ret_buff[i].valueType)
                        {
                        case DW_CUSTOM_PROP_VAL_TYPE_EMPTY:
                            printf("empty value \n");
                            break;
                        case DW_CUSTOM_PROP_VAL_TYPE_DOUBLE:
                            printf("double value:%lf\n", typed_ret_buff[i].value.doubleVal);
                            break;
                        case DW_CUSTOM_PROP_VAL_TYPE_INT64:
                            printf("number value:%I64d\n", typed_ret_buff[i].value.int64Val);
                            break;
                        case DW_CUSTOM_PROP_VAL_TYPE_STRING:
                            printf("string value:%s\n", typed_ret_buff[i].value.strVal);
                            break;
                        }
                    }

                    free(ret_buff);
                }
            }

            //Get Array info
            printf("\nARRAY INFO:\n");
            for (i = 0; i < ch_list_cnt; i++)
                if (ch_list[i].array_size > 1)
                {
                    array_info_cnt = DWGetArrayInfoCount(ch_list[i].index);
                    array_info_list = (struct DWArrayInfo*)malloc(sizeof(struct DWArrayInfo) * array_info_cnt);
                    DWGetArrayInfoList(ch_list[i].index, array_info_list);
                    for (j = 0; j < array_info_cnt; j++)
                    {
                        printf("Ch=%s AxisName=%s AxisUnit=%s AxisSize=%i\n", ch_list[i].name, array_info_list[j].name, array_info_list[j].unit, array_info_list[j].size);
                        for (k = 0; k < array_info_list[j].size; k++)
                        {
                            DWGetArrayIndexValue(ch_list[i].index, j, k, str, 255);
                            printf("Index=%i Value=%s\n", k, str);
                        }
                    }
                    free(array_info_list);
                }

            //Get DB data for all channels
            printf("\nFULL SPEED DATA:\n");
            for (i = 0; i < ch_list_cnt; i++)
            {
                printf("\nChannel=%s \n", ch_list[i].name);
                sample_cnt = DWGetScaledSamplesCount(ch_list[i].index);//get channel sample count
                if (sample_cnt < 0)
                    printf("\nERROR\n");
                data = (double*)malloc(sizeof(double) * sample_cnt * ch_list[i].array_size);
                time_stamp = (double*)malloc(sizeof(double) * sample_cnt);
                if (DWGetScaledSamples(ch_list[i].index, 0, sample_cnt, data, time_stamp) != DWSTAT_OK)//Get channel data
                    printf("\nERROR\n");
                //			printout
                //			for(j = 0; j < sample_cnt; j++)
                //				for (k = 0; k < ch_list[i].array_size; k++)
                //					printf("Time=%f\tValue=%f\n", time_stamp[j], data[j * ch_list[i].array_size + k]);

                free(data);
                free(time_stamp);
                printf("\n");
            }

            //Get raw DB data for all channels
            printf("\nFULL SPEED RAW DATA:\n");
            for (i = 0; i < ch_list_cnt; i++)
            {
                printf("\nChannel=%s \n", ch_list[i].name);
                sample_cnt = DWGetRawSamplesCount(ch_list[i].index); //get channel sample count
                if (sample_cnt < 0)
                    printf("\nERROR\n");

                success = 1;
                switch (data_types[i])
                {
                case dtByte:
                    raw_data = (uint8_t*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtShortInt:
                    raw_data = (int8_t*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtSmallInt:
                    raw_data = (short*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtWord:
                    raw_data = (uint16_t*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtInteger:
                    raw_data = (int*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtSingle:
                    raw_data = (float*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtInt64:
                    raw_data = (int64_t*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtDouble:
                    raw_data = (double*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtLongword:
                    raw_data = (uint32_t*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtCANPortData:
                    raw_data = (struct DWCANPortData*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                case dtCANFDPortData:
                    raw_data = (struct DWCANFDPortData*)malloc(data_type_lens[i] * sample_cnt * ch_list[i].array_size);
                    break;
                default:
                    success = 0;
                    break;
                }

                if (success == 1)
                {
                    time_stamp = (double*)malloc(sizeof(double) * sample_cnt);
                    if (DWGetRawSamples(ch_list[i].index, 0, sample_cnt, raw_data, time_stamp) != DWSTAT_OK) // get channel raw data
                        printf("\nERROR\n");
                    //			printout
                    //			for(j = 0; j < sample_cnt; j++) 
                    //				for (k = 0; k < ch_list[i].array_size; k++)
                    //				{
                    //					short tmp;
                    //					int itmp;
                    //					float ftmp;
                    //					double dtmp;
                    //			
                    //					switch (data_types[i])
                    //					{
                    //						// cast raw_data to a particular data type: dtByte, dtShort, dtSmallInt...
                    //						case dtSmallInt:
                    //							tmp = ((short*)raw_data)[j * ch_list[i].array_size + k];
                    //							printf("Time=%f\tValue=%d\n", time_stamp[j], tmp);
                    //							break;
                    //						case dtInteger:
                    //							itmp = ((int*)raw_data)[j * ch_list[i].array_size + k];
                    //							printf("Time=%f\tValue=%d\n", time_stamp[j], itmp);
                    //							break;
                    //						case dtSingle:
                    //							ftmp = ((float*)raw_data)[j * ch_list[i].array_size + k];
                    //							printf("Time=%f\tValue=%f\n", time_stamp[j], ftmp);
                    //							break;
                    //						case dtDouble:
                    //							dtmp = ((double*)raw_data)[j * ch_list[i].array_size + k];
                    //							printf("Time=%f\tValue=%f\n", time_stamp[j], dtmp);
                    //							break;
                    //					}
                    //				}

                    free(raw_data);
                    free(time_stamp);
                    printf("\n");
                }
            }

            free(data_types);
            free(data_type_lens);

            //Get binary data
            printf("\nBINARY DATA:\n");
            for (i = 0; i < ch_list_cnt; i++)
            {
                if (ch_list[i].data_type != dtBinary)
                    continue;

                printf("\nChannel=%s \n", ch_list[i].name);
                sample_cnt = DWGetBinarySamplesCount(ch_list[i].index);
                if (sample_cnt < 0)
                    printf("\nERROR\n");
                binData = (char*)malloc(sizeof(char) * MAX_BIN_LEN * MAX_BIN_SAMPLES);
                time_stamp = (double*)malloc(sizeof(double) * MAX_BIN_SAMPLES);

                //read sample by sample
                for (j = 0; j < sample_cnt; j++)
                {
                    read_samples = 1;
                    data_len = MAX_BIN_LEN;
                    if (DWGetBinarySamples(ch_list[i].index, j, binData, time_stamp, &data_len) == DWSTAT_OK)
                    {
                        printf("Time=%f, Size=%i Hex values=", time_stamp[0], data_len);
                        for (k = 0; k < data_len; k++)
                            printf("%02X ", (unsigned int)(unsigned char)binData[k]);
                        printf("\n\n");
                    }
                    else
                        printf("\nERROR\n\n");
                }

                //read n-samples (size(int)+sample(char))
                for (j = 0; j < sample_cnt; j += MAX_BIN_SAMPLES)
                {
                    read_samples = (sample_cnt - j < MAX_BIN_SAMPLES) ? sample_cnt - j : MAX_BIN_SAMPLES;
                    data_len = MAX_BIN_LEN * MAX_BIN_SAMPLES;
                    if (DWGetBinarySamplesEx(ch_list[i].index, j, read_samples, binData, time_stamp, &data_len) == DWSTAT_OK)
                    {//BinLen+BinData, BinLen+BinData, BinLen+BinData, BinLen+BinData... 
                        data_offset = 0;
                        for (k = 0; k < read_samples; k++)
                        {
                            data_len = *(int*)&binData[data_offset];
                            data_offset += 4;
                            printf("Time=%f, Size=%i Hex values=", time_stamp[k], data_len);
                            for (l = 0; l < data_len; l++)
                                printf("%02X ", (unsigned int)(unsigned char)binData[data_offset + l]);
                            data_offset += data_len;
                            printf("\n\n");
                        }
                    }
                    else
                        printf("\nERROR\n\n");
                }

                free(binData);
                free(time_stamp);
                printf("\n");
            }

            //Get reduced data from all channels
            printf("\nREDUCED DATA:\n");
            for (i = 0; i < ch_list_cnt; i++)
            {
                printf("Channel=%s \n", ch_list[i].name);
                if (DWGetReducedValuesCount(ch_list[i].index, &reduced_data_cnt, &block_size) != DWSTAT_OK) //get channel sample count
                    printf("\nERROR\n");
                if (reduced_data_cnt < 0)
                    printf("\nERROR\n");
                reduced_data = (struct DWReducedValue*)malloc(sizeof(struct DWReducedValue) * reduced_data_cnt);
                if (DWGetReducedValues(ch_list[i].index, 0, reduced_data_cnt, reduced_data) != DWSTAT_OK)
                    printf("\nERROR\n");
                //			printout
                //			for(j = 0; j < reduced_data_cnt; j++)//prints reduced data
                //				printf("Time=%f Ave=%f \n", reduced_data[j].time_stamp, reduced_data[j].ave);
                free(reduced_data);
                printf("\n");
            }

            //Get reduced block data from all channels
            printf("\nREDUCED BLOCK DATA:\n");
            const int IB_LEVEL = 0;
            int sample_total, sample_so_far;
            int sample_iter = 10000;
            int* ch_ids = (int*)malloc(sizeof(int) * ch_list_cnt);
            int ch_ids_len = 0;
            for (int i = 0; i < ch_list_cnt; i++) {
                ch_ids[i] = ch_list[i].index;
            }
            if (DWGetReducedValuesCount(ch_list[0].index, &sample_total, &block_size) != DWSTAT_OK) //get channel sample count
                printf("\nERROR\n");
            sample_so_far = 0;
            reduced_data = (struct DWReducedValue*)malloc(sizeof(struct DWReducedValue) * sample_iter * ch_list_cnt);
            while (sample_so_far < sample_total) {
                sample_iter = min(sample_iter, sample_total - sample_so_far);
                if (DWGetReducedValuesBlock(ch_ids, ch_list_cnt, sample_so_far, sample_iter, IB_LEVEL, reduced_data) != DWSTAT_OK)
                    printf("\nERROR\n");
                sample_so_far += sample_iter;
            }
            printf("\n");
            free(ch_list);

            //Get all complex channels
            printf("\nCOMPLEX CHANNELS:\n");
            ch_list_cnt = DWGetComplexChannelListCount();//get complex channels count
            ch_list = (struct DWChannel*)malloc(sizeof(struct DWChannel) * ch_list_cnt);
            DWGetComplexChannelList(ch_list);// get all complex channels
            for (i = 0; i < ch_list_cnt; i++)
                printf("Index=%i Name=%s Unit=%s Description=%s\n", ch_list[i].index, ch_list[i].name, ch_list[i].unit, ch_list[i].description);

            //Get DB data for all complex channels
            printf("\nFULL SPEED COMPLEX DATA:\n");
            for (i = 0; i < ch_list_cnt; i++)
            {
                printf("\nComplex Channel=%s \n", ch_list[i].name);
                sample_cnt = DWGetComplexScaledSamplesCount(ch_list[i].index);//get channel sample count
                if (sample_cnt < 0)
                    printf("\nERROR\n");
                complex_data = (struct DWComplex*)malloc(sizeof(struct DWComplex) * sample_cnt * ch_list[i].array_size);
                time_stamp = (double*)malloc(sizeof(double) * sample_cnt);
                if (DWGetComplexScaledSamples(ch_list[i].index, 0, sample_cnt, complex_data, time_stamp) != DWSTAT_OK)//Get channel data
                    printf("\nERROR\n");
                //			printout
                //			for(j = 0; j < sample_cnt; j++)
                //				for (k = 0; k < ch_list[i].array_size; k++)
                //					printf("Time=%f\tIm=%f\tRe=%f\n", time_stamp[j], complex_data[j * ch_list[i].array_size + k].im, complex_data[j * ch_list[i].array_size + k].re);

                free(complex_data);
                free(time_stamp);
                printf("\n");
            }
            free(ch_list);
            //Get Binary channels
            printf("\nTEXT CHANNELS:\n");
            ch_list_cnt = DWGetBinChannelListCount();//get text channels count
            ch_list = (struct DWChannel*)malloc(sizeof(struct DWChannel) * ch_list_cnt);
            DWGetBinChannelList(ch_list);// get all text channels
            for (i = 0; i < ch_list_cnt; i++)
            {
                printf("\nChannel=%s \n", ch_list[i].name);
                sample_cnt = DWGetScaledSamplesCount(ch_list[i].index);//get binary channel sample count		
                bin_recs = (struct TBinaryData*)malloc(sizeof(struct TBinaryData) * sample_cnt);
                time_stamp = (double*)malloc(sizeof(double) * sample_cnt);
                DWGetBinRecSamples(ch_list[i].index, 0, sample_cnt, bin_recs, time_stamp);
                //printout
                char text_buf[512];
                for (j = 0; j < sample_cnt; j++) {
                    __int64 pos = 0;
                    DWGetBinData(ch_list[i].index, bin_recs + j, text_buf, &pos, 512);
                    text_buf[bin_recs[j].Size] = 0;
                    printf("Time=%f\tValue=%s\n", time_stamp[j], text_buf);
                }

                free(bin_recs);
                free(time_stamp);
                printf("\n");
            }
            free(ch_list);

            if (DWCloseDataFile() != DWSTAT_OK)//close dewesoft data file
                printf("\nERROR: DWCloseDataFile\n");
        }

        printf("\n\n\n");
    }

    if (DWDeInit() != DWSTAT_OK)//clear dll
        printf("\nERROR: DWDeInit\n");

    if (CloseDWDLL() != 0)//close dll
        printf("\nERROR: CloseDWDLL\n");

    for (i = 0; i < argc - 1; i++)
        free(filenames[i]);
    free(filenames);

    return 0;
}
