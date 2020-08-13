1. sudo -s (루트 로 이동) / su - 사용자이름  입력하면 원래대로 돌아옴
2. cd /mnt/ (mnt로 이동)
3. ./hello /mnt/otfs/ 하면 마운트 된다.
4. ./hello /mnt/otfs/ -d 로 해두고, 새 터미널 창을 열어 작동하면 fuse가 어떻게 동작하는지 확인할 수 있다.
5. 새 터미널 창을 열어 sudo -s, cd /mnt/otfs 로 들어가서 mkdir이나 등등 하면된다.
6. 새 터미널 창에서 /mnt 폴더에서 umount /mnt/otfs 하면 언마운트 된다.
7. gdb ./hello 하고,
8. gdb 창에서 r /mnt/otfs -d 로 들어가면 gdb와 동시에 fuse 동작 과정을 확인할 수 있다.

명령어 bt(back trace) 로 어디서 에러가 났는지 확인 가능하다.

하고 꼭 umount /mnt/otfs 를 통해 언마운트를 해줘야한다.

![image-20200730173814247](C:\Users\win\AppData\Roaming\Typora\typora-user-images\image-20200730173814247.png)

![image-20200730173828561](C:\Users\win\AppData\Roaming\Typora\typora-user-images\image-20200730173828561.png)



새창

![image-20200730173838129](C:\Users\win\AppData\Roaming\Typora\typora-user-images\image-20200730173838129.png)





왼쪽

gcc 를 해줌

gdb ./hello

r /mnt/otfs -d -s

종료는 ctrl c

, q로 gdb나감



오른쪽

왼쪽에서 r /mnt/otfs -d -s 한 다음에 cd otfs  로 들어가기 

그 뒤에 mkdir 이나 ls 명령어 치면 왼쪽에 어떤 함수가 실행되는지 알수있음

종료할 땐 꼭 cd /mnt 한 뒤, umount /mnt/otfs 해주기 