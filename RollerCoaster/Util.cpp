#include "Util.h";

#define _CRT_SECURE_NO_WARNINGS
#include <fstream>
#include <sstream>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION


unsigned int compileShader(GLenum type, const char* source)
{
    // citanje source koda iz fajla
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;

    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }

    std::string temp = ss.str();
    const char* sourceCode = temp.c_str(); // kod shadera iz source fajla

    // kreiranje praznog shadera (vertex ili fragment)
    int shader = glCreateShader(type);

    int success;
    char infoLog[512];

    glShaderSource(shader, 1, &sourceCode, NULL); // ubacivanje source koda
    glCompileShader(shader);                      // kompajliranje shadera

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success); // provjera kompajliranja
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog); // ispis greske
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}

unsigned int createShader(const char* vsSource, const char* fsSource)
{
    // kreiranje shadera koji ce koristiti vertex i fragment shader
    unsigned int program;
    unsigned int vertexShader;
    unsigned int fragmentShader;

    program = glCreateProgram(); // kreiranje praznog shadera

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource);   // kompajliranje vertex shadera
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource); // kompajliranje fragment shadera

    // kacenje oba shadera na objedinjeni program
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);      // povezivanje shadera u jedan program
    glValidateProgram(program);  // provjera da li program radi

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);

    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    // brisanje pojedinacnih shadera
    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}
