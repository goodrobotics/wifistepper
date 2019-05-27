module Jekyll
    class MDIcon < Liquid::Tag
        def initialize(tag_name, settings, tokens)
            super
            cmds = settings.split(',').map { |c| c.strip }
            @name = cmds[0]
            @cls = cmds.select { |c| c[0] == '.' }.map { |c| " #{c[1..-1]}"}.join
            
            colorcode = cmds.select { |c| c[0] == '#' }.first
            @color = if not colorcode.nil?
                @cls << " mdicolor"
                " style=\"fill: #{colorcode};\""
            else
                ""
            end
        end

        def render(context)
            path = File.join(Jekyll.sites.first.config['source'], "_includes", "mdicons", "#{@name}.svg")
            path = File.join(Jekyll.sites.first.config['source'], "_includes", "usericons", "#{@name}.svg") if not File.exists?(path)
            
            if not File.exists?(path)
                return ""
            end

            data = IO.read(path)
            return "<span class=\"mdicon#{@cls}\"#{@color}>#{data}</span>"
        end
    end
end

Liquid::Template.register_tag('mdicon', Jekyll::MDIcon)
