#!/usr/bin/env python3
import os
import cgi

print("Content-Type: text/html; charset=utf-8\r\n\r\n")  # 인코딩 명시

form = cgi.FieldStorage()
name = form.getvalue("name", "익명")

print(f"<html><body><h2>안녕하세요, {name}님!</h2></body></html>")