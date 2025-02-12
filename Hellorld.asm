start:
  lxi h, message
loop:
  mov a,m
  out 1
  inx h
  cpi 0
  jnz loop
  hlt
message:
  db 72, 101, 108, 108, 111, 114, 108, 100, 33, 10, 13, 0 
