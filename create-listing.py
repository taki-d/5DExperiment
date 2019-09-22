import os


def collect_file_list(path, file_extensions):
    file_list = []

    for n in os.listdir(path):
        if os.path.isfile(os.path.join(path, n)):
            if n.split('.')[-1] in file_extensions:
                file_list.append(os.path.join(path, n))
        else:
            collected_files = collect_file_list(os.path.join(path, n), file_extensions)
            for k in collected_files:
                file_list.append(k)

    return file_list


if __name__ == '__main__':
    collect_file_extensions = ['h', 'cpp', 'html', 'js']
    files = collect_file_list('./libraries', collect_file_extensions)

    output = open('sourcecode.tex', 'w')

    for file_path in files:
        f = open(file_path)
        output.write('\\begin{lstlisting}[caption=' + file_path + ',label=' + file_path + ']{ \n' + f.read() + '\n}\n')
        f.close()

    output.close()


