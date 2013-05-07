# Grab files from a remote location for inclusion into FCM
require 'tempfile'

module FCM
  module Action
    class FetchError < StandardError
    end

    class Fetch
      attr_accessor :method, :getter
      def initialize(uri)
        @method = uri.scheme
        @uri = uri
        @getter = "get_#{@method}"
        unless self.respond_to?(@getter.to_sym)
          raise FCM::Action::FetchError, "Unsupported fetch method: #{@method}"
        end
      end

      def get
        return self.send(@getter)
      end

      def get_http
        require 'net/http'
        req = Net::HTTP::Get.new(@uri.path)
        http = Net::HTTP.new(@uri.host, @uri.port)
        resp = http.request(req)
        return http_response(resp)
      end

      def get_https
        require 'net/https'
        req = Net::HTTP::Get.new(@uri.path)
        http = Net::HTTP.new(@uri.host, @uri.port)
        http.use_ssl = true
        http.ca_path = get_ca_path
        http.verify_mode = OpenSSL::SSL::VERIFY_PEER
        http.verify_depth = 5
        resp = http.request(req)
        return http_response(resp)
      end

      def http_response(resp)
        if resp.code != '200'
          raise FCM::Action::FetchError, "Invalid response code #{resp.code} for #{@uri}"
        end
        return resp.body
      end

      def get_ca_path(paths=[])
        paths.push('/data/app/secrets', '/etc/ssl/certs')
        paths.each do |p|
          if File.exists?(p) and File.directory?(p)
            return p
          end
        end
      end
    end
  end
end
