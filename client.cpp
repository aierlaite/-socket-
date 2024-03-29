#include<stdio.h>
#include<string.h>
#include<time.h>
#include<conio.h>
#include<Windows.h>
#pragma comment(lib,"Ws2_32.lib")

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR_CODE 5
#define DATA_SIZE 512
#define TIME_OUT 2
#define MAX_RETRANSMISSION 100

FILE* log_file;
time_t nowtime;
clock_t start, end;

void download(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen);
int read_request(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen);
int receive_data(char* recv_buffer, SOCKET sock, sockaddr_in& addr, int addrlen);
int write_request(int mode, const char* filename, char* buffer, SOCKET serversock, sockaddr_in addr, int addrLen);
int receiveACK(char* buffer, SOCKET serversock, sockaddr_in &addr, int addrlen);
void upload(int mode, const char* filename, char* buffer, SOCKET serversock, sockaddr_in addr, int addrlen);
int send_data(SOCKET serversock, sockaddr_in addr, int addrlen, FILE* fp, char* buffer, char* data, int data_size, unsigned short block_num);
int send_ACK(SOCKET sock, sockaddr_in addr, int addrlen, FILE* fp, char* buffer, char* data, int data_size, unsigned short block_num);
void ending2(int recv_bytes,clock_t end,clock_t start);
void ending1(int recv_bytes,clock_t end,clock_t start);




void ending1(int recv_bytes,clock_t end,clock_t start)
{
	int result;
    printf("������� �����С:%dbytes speed:%.3fkb/s", recv_bytes, recv_bytes / (1024 * (double)(end - start) / CLK_TCK));
    printf("\n�����������...");
    result = getch();
    return;
}

void ending2(int send_bytes,clock_t end,clock_t start)
{
	int result;
    printf("������� �����С:%dbytes speed:%.3fkb/s", send_bytes, send_bytes / (1024 * ((double)(end - start) / CLK_TCK)));
	printf("\n�����������...");
	result = getch();
	return;
}

/**
 * @brief �ӷ����������ļ�
 * 
 * @param mode ����ģʽ��1��ʾ�ı�ģʽ��2��ʾ������ģʽ
 * @param filename �ļ���
 * @param buffer ������
 * @param sock �׽���
 * @param addr ��������ַ�ṹ
 * @param addrlen ��ַ�ṹ����
 */
void download(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen) {
    int recv_bytes = 0, max_send = 0, result, data_size;
    sockaddr_in serveraddr = { 0 };
    long long int block_num = 0;
    char data[DATA_SIZE];
    char recv_buffer[DATA_SIZE*2];
    BOOL end_flag = FALSE;
    BOOL start_flag = TRUE;
    FILE* fp;
    // ���ݴ���ģʽ���ļ�
    if (mode == 1)
        fp = fopen(filename, "w");
    if (mode == 2)
        fp = fopen(filename, "wb");
    // �ж��ļ����Ƿ�ɹ�
    if (fp == NULL) {
        printf("�ļ������ڻ����ļ��޷���\n�����������...");
        result = getch();
        return;
    }
    // ���Ͷ�ȡ����
    read_request(mode, filename, buffer, sock, addr, addrlen);
    while (1) {
        // ��ʼ��ʱ
        if (start_flag) {
            start = clock();
            start_flag = FALSE;
        }
        // �ж��Ƿ������
        if (end_flag) {
            ending1(recv_bytes, end, start);
            fclose(fp);
            return;
        }
        // �������ݰ�
        result = receive_data(recv_buffer, sock, serveraddr, addrlen);
        // �жϽ��ս��
        if (result > 0) {
            max_send = 0; // �����ش�����
            // �ж�����Ƿ�����
            if (block_num != ((recv_buffer[2] << 8) + recv_buffer[3] - 1))
                result = -1;
        }
        // ������յ������ݰ�
        if (result > 0) {
            recv_bytes += result - 4; // ��¼�������ݴ�С
            max_send = 0; // �����ش�����
            block_num++;
            data_size = fwrite(recv_buffer + 4, 1, result - 4, fp);
            if (data_size < 512) {
                // �������
                end_flag = TRUE;
                end = clock();
            }
            // ���� ACK ��
            result = send_ACK(sock, serveraddr, addrlen, fp, buffer, data, data_size, block_num);
        }
        // ����ʱ����ʧ��
        else if (result == -1) {
            max_send++; // �ش�������һ
            printf("...��%d��ACK���ش���...%d\n",block_num, max_send);
            if (max_send > MAX_RETRANSMISSION) {
                printf("�ش���������\n�����������...");
                result = getch();
                fprintf(log_file, "����:�ش��������� %s", asctime(localtime(&(nowtime = time(NULL))))); 
                return ;
            }
            // �ش� ACK ��
            if (block_num > 0) {
                fprintf(log_file, "�ش�ACK�� ACK���:%d %s", block_num, asctime(localtime(&(nowtime = time(NULL)))));
                send_ACK(sock, serveraddr, addrlen, fp, buffer, data, data_size, block_num);
            }
            // �ش���ȡ����
            else {
                fprintf(log_file, "�ش�RRQ���� %s", asctime(localtime(&(nowtime = time(NULL)))));
                read_request(mode, filename, buffer, sock, addr, addrlen);
            }
        }
        // ������յ��Ĵ����
        else {
            printf("ERROR!������:%d %s", recv_buffer[3], recv_buffer + 4);
            printf("\n�����������...");
            result = getch();
            return;
        }
    }
}

/**
 * @brief ���Ͷ�ȡ���󵽷�����
 * 
 * @param mode ����ģʽ��1��ʾ�ı�ģʽ��2��ʾ������ģʽ
 * @param filename �ļ���
 * @param buffer ������
 * @param sock �׽���
 * @param addr ��������ַ�ṹ
 * @param addrlen ��ַ�ṹ����
 * @return int ���ط��ͽ����SOCKET_ERROR��ʾ����ʧ�ܣ�������ʾ���͵��ֽ���
 */
int read_request(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen) {
    int send_size = 0;
    int result;
    memset(buffer, 0, sizeof(buffer));  // ��ջ�����
    if (mode == 1) {
        send_size = sprintf(buffer,"%c%c%s%c%s%c",0,1,filename,0,"netascii",0);  // �����ı�ģʽ�Ķ�ȡ��������
    }
    else {
        send_size = sprintf(buffer,"%c%c%s%c%s%c",0,1,filename,0,"octet",0);  // ���ɶ�����ģʽ�Ķ�ȡ��������
    }
    result = sendto(sock, buffer, send_size, 0, (struct sockaddr*)&addr, addrlen);  // �������󵽷�����
    if (result == SOCKET_ERROR) {
        printf("����RRQ����ʧ��\n�����������...");
        result = getch();
        // ��¼��������ʧ�ܵĴ�����Ϣ
        fprintf(log_file, "����:����RRQ����ʧ��	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
    }
    else {
        // ��¼��������ɹ�����Ϣ
        fprintf(log_file, "����RRQ����ɹ�	send%dbytes	�ļ���:%s	%s", result, filename, asctime(localtime(&(nowtime = time(NULL)))));
    }
    return result;
}


/**
 * ��������
 * @param recv_buffer ���ջ�����
 * @param sock �׽���
 * @param addr ��ַ
 * @param addrlen ��ַ����
 * @return �������ݽ��
 */
int receive_data(char* recv_buffer, SOCKET sock, sockaddr_in& addr, int addrlen) {
    memset(recv_buffer, 0, sizeof(recv_buffer));
    struct timeval tv;
    fd_set readfds;
    int result;
    int wait_time;
    for (wait_time = 0; wait_time < TIME_OUT; wait_time++) 
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select(sock + 1, &readfds, NULL, NULL, &tv);
        result = recvfrom(sock, recv_buffer, DATA_SIZE*2, 0, (struct sockaddr*)&addr, (int*)&addrlen);
        if (result > 0 && result < 4) {
            printf("bad packet\n�����������...");
            result = getch();
            fprintf(log_file, "����:���հ�����ȷ	%s", asctime(localtime(&(nowtime = time(NULL)))));
            return 0;
        }
        else if (result >= 4) {
            if (recv_buffer[1] == ERROR_CODE) {
                fprintf(log_file, "����:���յ������	������:%d	������Ϣ%s	%s", recv_buffer[3], recv_buffer + 4, asctime(localtime(&(nowtime = time(NULL)))));
                return -2;
            }
            fprintf(log_file, "�������ݳɹ�	receive%dbytes	���ݰ����:%d	%s", result, recv_buffer[3] + (recv_buffer[2] >> 8), asctime(localtime(&(nowtime = time(NULL)))));
            return result;
        }
    }
    if (wait_time >= TIME_OUT) {
        fprintf(log_file, "����:�ȴ����ճ�ʱ		%s", asctime(localtime(&(nowtime = time(NULL)))));
        return -1;
    }
}
/**
 * ����ACK��
 * @param sock �׽���
 * @param addr ��ַ
 * @param addrlen ��ַ����
 * @param fp �ļ�ָ��
 * @param buffer ������
 * @param data ����
 * @param data_size ���ݴ�С
 * @param block_num ���ݿ���
 * @return ���ͽ��
 */
int send_ACK(SOCKET sock, sockaddr_in addr, int addrlen, FILE* fp, char* buffer, char* data, int data_size, unsigned short block_num) {
    int result;
    int send_size = 0;
    memset(buffer, 0, sizeof(buffer));
    buffer[++send_size] = ACK;
    buffer[++send_size] = (char)(block_num >> 8);
    buffer[++send_size] = (char)block_num;
    result = sendto(sock, buffer, 4, 0, (struct sockaddr*)&addr, addrlen);
    if (result == SOCKET_ERROR) {
        // ����ACKʧ�ܴ���
        printf("����ACKʧ��\n�����������...");
        result = getch();
        fprintf(log_file, "����:ACK������ʧ��	ACK���:%d	������:%d	%s", block_num, WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
        return -1;
    }
    else {
        fprintf(log_file, "ACK�����ͳɹ�	send%dbytes	ACK�����:%d	%s", result, block_num, asctime(localtime(&(nowtime = time(NULL)))));
        return result;
    }
}


/**
 * �ϴ��ļ�
 * @param mode ģʽ��1 - netascii, 2 - octet��
 * @param filename �ļ���
 * @param buffer ������
 * @param sock �׽���
 * @param addr ��ַ
 * @param addrlen ��ַ����
 */
void upload(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen) {
    int send_bytes = 0, result;
    sockaddr_in serveraddr = { 0 };
    int max_send = 0;
    int data_size;
    long long int block_num = 0;
    char data[DATA_SIZE];
    char recv_buffer[DATA_SIZE * 2];
    BOOL end_flag = FALSE;
    BOOL start_flag = TRUE;
    FILE* fp;

    if (mode == 1)
        fp = fopen(filename, "r");
    if (mode == 2)
        fp = fopen(filename, "rb");

    if (fp == NULL) {
        // ���ļ�ʧ�ܴ���
        printf("���ļ�ʧ��\n�����������...");
        result = getch();
        return;
    }

    // ����д����
    write_request(mode, filename, buffer, sock, addr, addrlen);

    while (1) {
        result = receiveACK(recv_buffer, sock, serveraddr, addrlen);
        if (result != block_num)
            result = -1;
        if (result >= 0) {
            max_send = 0;
            if (end_flag) {
                ending2(send_bytes, end, start);
                fclose(fp);
                return;
            }
            block_num = result;
            memset(data, 0, DATA_SIZE);
            data_size = fread(data, 1, DATA_SIZE, fp);
            fprintf(log_file, "��%s�ļ���ȡ��	size:%dbytes			%s", filename, data_size, asctime(localtime(&(nowtime = time(NULL)))));
            if (start_flag == 1) {
                start = clock();
                start_flag = FALSE;
            }
            result = send_data(sock, serveraddr, addrlen, fp, buffer, data, data_size, ++block_num);
            send_bytes += data_size;
            if (data_size < 512 && result != -1) 
            {
                end = clock();
                end_flag = TRUE;
            }
        }
        else if ((result == -1) || (result == -2))
        {
            max_send++;
            printf("...��%d�����ݰ������ش���...%d\n", block_num, max_send);
            if (max_send > MAX_RETRANSMISSION) {
                // �ش������������ƴ���
                printf("�ش������������õ�����\n�����������...");
                result = getch();
                fprintf(log_file, "����:�ش�������������	%s", asctime(localtime(&(nowtime = time(NULL)))));
                return;
            }
            if (block_num > 0) {
                fprintf(log_file, "�ش����ݰ�	���ݰ����:%d	%s", block_num, asctime(localtime(&(nowtime = time(NULL)))));
                send_data(sock, serveraddr, addrlen, fp, buffer, data, data_size, block_num);
            }
            else {
                fprintf(log_file, "�ش�WRQ����	%s", asctime(localtime(&(nowtime = time(NULL)))));
                write_request(mode, filename, buffer, sock, addr, addrlen);
            }
        }
        else if (result == -3) {
            // ������Ϣ����
            printf("ERROR!�������:%d %s \n�����������...", recv_buffer[3], recv_buffer + 4);
            result = getch();
            return;
        }
    }
}

/**
 * ����д����
 * @param mode ģʽ��1 - netascii, 0 - octet��
 * @param filename �ļ���
 * @param buffer ������
 * @param sock �׽���
 * @param addr ��ַ
 * @param addrlen ��ַ����
 * @return ���ͽ��
 */
int write_request(int mode, const char* filename, char* buffer, SOCKET sock, sockaddr_in addr, int addrlen) {
    int send_size = 0;
    int result;
    memset(buffer, 0, sizeof(buffer));
    if (mode == 1) {
        // netascii ģʽ
        send_size = sprintf(buffer, "%c%c%s%c%s%c", 0, 2, filename, 0, "netascii", 0); 
    } else {
        // octet ģʽ
        send_size = sprintf(buffer, "%c%c%s%c%s%c", 0, 2, filename, 0, "octet", 0); 
    }
    result = sendto(sock, buffer, send_size, 0, (struct sockaddr*)&addr, addrlen);
    if (result == SOCKET_ERROR) {
        // ����ʧ�ܴ���
        printf("����WRQ����ʧ��\n�����������...");
        result = getch();
        fprintf(log_file, "����:����WRQ����ʧ��	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
    } else {
        // ���ͳɹ�����
        fprintf(log_file, "����WRQ����ɹ�	send%dbytes	�ļ���:%s	%s", result, filename, asctime(localtime(&(nowtime = time(NULL)))));
    }
    return result;
}

/**
 * ���� ACK ���ݰ�
 * @param recv_buffer ���ջ�����
 * @param sock �׽���
 * @param addr ��ַ
 * @param addrlen ��ַ����
 * @return ���ս��
 */
int receiveACK(char* recv_buffer, SOCKET sock, sockaddr_in& addr, int addrlen) {
    memset(recv_buffer, 0, sizeof(recv_buffer));
    struct timeval tv;
    fd_set readfds;
    int result;
    int wait_time;
    for (wait_time = 0; wait_time < TIME_OUT; wait_time++) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select(sock + 1, &readfds, NULL, NULL, &tv);
        result = recvfrom(sock, recv_buffer, 4, 0, (struct sockaddr*)&addr, (int*)&addrlen);
        if (result > 0 && result < 4) {
            // ���������ݰ�
            printf("bad packet\n�����������...");
            result = getch();
            fprintf(log_file, "����:���հ�����ȷ	%s", asctime(localtime(&(nowtime = time(NULL)))));
            return -2;
        }
        else if (result >= 4) {
            // ���յ��������ݰ�
            if (recv_buffer[1] == ERROR_CODE) {
                // ���������
                fprintf(log_file, "����:���յ������	������:%d	������Ϣ%s	%s", recv_buffer[3], recv_buffer + 4, asctime(localtime(&(nowtime = time(NULL)))));
                return -3;
            }
            // ���ճɹ�����
            fprintf(log_file, "����ACK���ɹ�	receive%dbytes	ACK���:%d	%s", result, recv_buffer[3] + (recv_buffer[2] << 8), asctime(localtime(&(nowtime = time(NULL)))));
            return recv_buffer[3] + (recv_buffer[2] << 8); // ���� ACK ���
        }
    }
    if (wait_time >= TIME_OUT) {
        // ���ճ�ʱ
        fprintf(log_file, "����:�ȴ����ճ�ʱ		%s", asctime(localtime(&(nowtime = time(NULL)))));
        return -1;
    }
}

/**
 * ����������ָ���׽���
 * @param sock �׽���
 * @param addr Ŀ���ַ
 * @param addrlen ��ַ����
 * @param fp �ļ�ָ��
 * @param buffer ������
 * @param data ����
 * @param data_size ���ݴ�С
 * @param block_num ���ݿ���
 * @return ���ͽ��
 */
int send_data(SOCKET sock, sockaddr_in addr, int addrlen, FILE* fp, char* buffer, char* data, int data_size, unsigned short block_num) {
    int result, send_size = 0;
    memset(buffer, 0, sizeof(buffer));
    buffer[++send_size] = DATA; // �������ݱ�ʶ
    buffer[++send_size] = (char)(block_num >> 8); // ���ݿ��Ÿ� 8 λ
    buffer[++send_size] = (char)block_num; // ���ݿ��ŵ� 8 λ
    ++send_size;
    memcpy(buffer + send_size, data, data_size); // ����������������
    send_size += data_size;
    buffer[send_size] = 0;
    // ��������
    result = sendto(sock, buffer, send_size, 0, (struct sockaddr*)&addr, addrlen);
    if (result == SOCKET_ERROR) {
        // ����ʧ�ܴ���
        printf("��������ʧ��\n�����������...");
        result = getch();
        fprintf(log_file, "����:��������ʧ��	���ݰ����:%d ������:%d	%s", block_num, WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
        return -1;
    }
    else {
        // ���ͳɹ�����
        fprintf(log_file, "�������ݰ��ɹ�	send%dbytes	���ݰ����:%d	%s", result, block_num, asctime(localtime(&(nowtime = time(NULL)))));
        return result;
    }
}

void unbind(SOCKET socket) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;  
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) 
	{printf("Socket unbound successfully.\n");
    } else {
        printf("Socket unbound successfully.\n");
    }
}

int main() {
	//��ʼ����־�ļ�
	log_file = fopen("log.txt", "w+");
	char filename[128],buffer[DATA_SIZE*2];//�ļ���
	int Result;//���淵��ֵ

	//����Winsocket
	WSADATA wsaData;
	Result = WSAStartup(0x0202, &wsaData);//WSAS�İ汾�� 
	if (Result!=0)
	{
		printf("WSAStartup failed with error: %d", Result);
		fprintf(log_file, "�����޷�����Winsocket	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
		return 0;
	}
	else{
		fprintf(log_file, "�Ѿ��ɹ�����Winsocket	%s", asctime(localtime(&(nowtime = time(NULL)))));
	} 
	

	//�����׽���
	iny:
	SOCKET client_sock;
	client_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client_sock == INVALID_SOCKET) {
		printf("�����׽���ʧ�ܣ�\n");
		fprintf(log_file, "����:�޷������׽���	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
		return 0;
	}
	else
	{
		fprintf(log_file, "�Ѿ������׽���	%s", asctime(localtime(&(nowtime = time(NULL)))));
	} 
	

	//����� ip�Ͷ˿� �ͻ���ip�Ͷ˿�
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	int clientport = 0;
	int serverport = 69;
	char serverip[20];
	char clientip[20]="127.0.0.1";
	printf("���������˵�ַ:");
	scanf("%s", serverip);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverport);
	server_addr.sin_addr.S_un.S_addr = inet_addr(serverip);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(clientport);
	client_addr.sin_addr.S_un.S_addr = inet_addr(clientip);


	unsigned long Opt = 1;
	Result = ioctlsocket(client_sock, FIONBIO, &Opt);
	if (Result == SOCKET_ERROR) {
		printf("���÷�����ģʽʧ��\n");
		fprintf(log_file, "ERROR:�޷����÷�����ģʽ	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
		return 0;
	}
	Result = bind(client_sock, (LPSOCKADDR)&client_addr, sizeof(client_addr));
	if (Result == SOCKET_ERROR)
	{
		// ��ʧ��
		printf("�󶨽ӿ�ʱ��������!\n�����������...");
		Result = getch();
		fprintf(log_file, "ERROR:�޷��󶨽ӿ�	������:%d	%s", WSAGetLastError(), asctime(localtime(&(nowtime = time(NULL)))));
		return 0;
	}

	//������
	char choice;
	char mode;
	inx: 
	while (1) {
		system("cls");
		printf("Ŀǰ���͵ķ�������ַΪ%s\n",serverip);
		printf("+------------------------------+\n");
		printf("|0.�ر�TFTP�ͻ���   1.�ϴ��ļ� |\n");
		printf("|2.�����ļ�  3.�������ip��ַ  | \n");
		printf("+------------------------------+\n");
		choice = getch();
		if (choice == '1') {
			printf("ĿǰģʽΪ���ϴ��ļ�\n�������ļ���:\n");
			scanf("%s", filename);
			printf("Ŀǰ״̬Ϊ���ϴ��ļ� %s \n",filename);
			printf("1.netascii\n2.octet\n��ѡ����ģʽ:\n");
			mode = getch();
			printf("...�ϴ���...\n");
			upload(mode-48, filename, buffer, client_sock, server_addr, sizeof(sockaddr_in));
		}
		if (choice == '2') {
			printf("ĿǰģʽΪ�������ļ�\n�������ļ���\n");
			scanf("%s", filename);
			printf("Ŀǰ״̬Ϊ�������ļ� %s \n",filename);
			printf("1.netascii\n2.octet\n��ѡ����ģʽ:\n");
			mode = getch();
			printf("...������...\n");
			download(mode-48, filename, buffer, client_sock, server_addr, sizeof(sockaddr_in));
		}
	if (choice == '0')
	{
		printf("ȷ���˳��𣿣�y/n��\n") ;
		char mode1;
			while(1)
			{
				mode1=getch();
				if(mode1=='y')
				{
				printf("+------------------------------+\n");
				printf("|      ��ӭ���´ν���ʹ��      |\n");
				printf("|     ���пƼ���ѧ����ѧԺ     |\n");
				printf("+------------------------------+\n");
				return 0;
				}
				else if(mode1=='n')
				{
					goto inx; 
				}
			}
			fclose(log_file);
	}
	if(choice == '3') 
	{
		system("cls"); 
		unbind(client_sock);
		closesocket(client_sock);
		goto iny;
	}
}
}




