version: '2'

services:
  {{ node['node_name'] }}:
        build: 
           context: ./
           dockerfile: ./Dockerfile
        image: {{ node['image_name'] }}
        hostname: "rxclient-$BUILD_ID"
        {%- if 'start_command' in node %}
        command: {{ node['start_command'] }}
        {%- endif %}        
        {%- if 'ports' in node and node['ports']|length > 0 %}
        ports:
        {%- if 'ssh_port' in node %}
            - "{{ node['ssh_port'] }}:{{ node['ssh_internal_port'] }}"
        {%- endif %}
        {%- for p in node['ports'] %}
            - "{{ p }}{% endfor %}"
        {%- endif %}
        {%- if 'cap_add' in node and node['cap_add']|length > 0 %}
        cap_add:
        {%- for v in node['cap_add'] %}
            - {{ v }}{% endfor %}
        {%- endif %}
        {%- if 'volumes' in node and node['volumes']|length > 0 %}
        volumes:
        {%- for v in node['volumes'] %}
            - {{ v }}{% endfor %}
        {%- endif %}
        {% if 'devices' in node and node['devices']|length > 0 %}
        devices:
        {%- for v in node['devices'] %}
            - {{ v }}{% endfor %}
        {%- endif %}
        {%- if 'environment' in node and node['environment']|length > 0 %}
        environment:
        {%- for v in node['environment'] %}
            - {{ v }}{% endfor %}
        {%- endif %}
        {% if 'env_file' in node and node['env_file']|length > 0 %}
        env_file:
        {%- for v in node['env_file'] %}
            - {{ v }}{% endfor %}
        {%- endif %}
        tty: true
        {% if 'extra_hosts' in project and project['extra_hosts']|length > 0 %}
        extra_hosts:
        {%- for host in project['extra_hosts'] %}
                - "{{ host['node_name'] }}: {{ host['ip'] }}"{% endfor %}
        {%- endif %}
        networks:
          rx-net:

networks:
    rx-net:
      external:
        name: rx-net
