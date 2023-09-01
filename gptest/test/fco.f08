program Hello_World
  implicit none
  character(len=20) :: name[*] ! scalar coarray, one "name" for each image.
  ! Note: "name" is the local variable while "name[<index>]" accesses the
  ! variable in a specific image; "name[this_image()]" is the same as "name".

  REAL VEC(5)
  DATA VEC /3*9.0, 0.1, -3.4/
  integer :: inst_num

  inst_num = this_image()
  VEC(inst_num) = VEC(inst_num) / 1.5
  ! calc avg
  write(*,*)'avg=',sum(VEC)/(max(1,size(VEC)))
  
  ! Interact with the user on Image 1; execution for all others pass by.
  if (inst_num == 1) then   
    write(*,'(a)',advance='no') 'Enter your name: '
    read(*,'(a)') name
  end if
  ! Distribute information to all images
  call co_broadcast(name,source_image=1)

  ! I/O from all images, executing in any order, but each record written is intact. 
  write(*,'(3a,i0)') 'Hello ',trim(name),' from image ', this_image()
end program Hello_world